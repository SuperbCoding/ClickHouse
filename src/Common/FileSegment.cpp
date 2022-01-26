#include "FileSegment.h"
#include <Common/FileCache.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int FILE_CACHE_ERROR;
}

FileSegment::FileSegment(
        size_t offset_,
        size_t size_,
        const Key & key_,
        FileCache * cache_,
        State download_state_)
    : segment_range(offset_, offset_ + size_ - 1)
    , download_state(download_state_)
    , file_key(key_)
    , cache(cache_)
{
    if (download_state == State::DOWNLOADED)
        reserved_size = downloaded_size = size_;
    else if (download_state != State::EMPTY)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Can create cell with either DOWNLOADED or EMPTY state");
}

FileSegment::State FileSegment::state() const
{
    std::lock_guard segment_lock(mutex);
    return download_state;
}

size_t FileSegment::downloadOffset() const
{
    std::lock_guard segment_lock(mutex);
    return range().left + downloaded_size - 1;
}

String FileSegment::getCallerId()
{
    if (!CurrentThread::isInitialized() || CurrentThread::getQueryId().size == 0)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Cannot use cache without query id");

    return CurrentThread::getQueryId().toString();
}

String FileSegment::getOrSetDownloader()
{
    std::lock_guard segment_lock(mutex);

    if (downloader_id.empty())
    {
        downloader_id = getCallerId();
        LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "Set downloader: {}, prev state: {}", downloader_id, toString(download_state));
        download_state = State::DOWNLOADING;
    }

    LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "Returning with downloader: {} and state: {}", downloader_id, toString(download_state));
    return downloader_id;
}

bool FileSegment::isDownloader() const
{
    std::lock_guard segment_lock(mutex);
    return getCallerId() == downloader_id;
}

void FileSegment::write(const char * from, size_t size)
{
    if (!size)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Writing zero size is not allowed");

    if (availableSize() < size)
        throw Exception(
            ErrorCodes::FILE_CACHE_ERROR,
            "Not enough space is reserved. Available: {}, expected: {}", availableSize(), size);

    if (!isDownloader())
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Only downloader can do the downloading");

    if (!download_buffer)
    {
        auto download_path = cache->path(key(), offset());
        download_buffer = std::make_unique<WriteBufferFromFile>(download_path);
    }

    download_buffer->write(from, size);
    downloaded_size += size;
}

FileSegment::State FileSegment::wait()
{
    std::unique_lock segment_lock(mutex);

    if (download_state == State::EMPTY)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Cannot wait on a file segment with empty state");

    if (download_state == State::DOWNLOADING)
    {
        LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "{} waiting on: {}", downloader_id, range().toString());

        assert(!downloader_id.empty() && downloader_id != getCallerId());

#ifndef NDEBUG
        std::lock_guard cache_lock(cache->mutex);
        assert(!cache->isLastFileSegmentHolder(key(), offset(), cache_lock));
#endif

        cv.wait_for(segment_lock, std::chrono::seconds(60)); /// TODO: pass through settings
    }

    return download_state;
}

bool FileSegment::reserve(size_t size)
{
    if (!size)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Zero space reservation is not allowed");

    std::lock_guard segment_lock(mutex);

    if (downloaded_size + size > range().size())
        throw Exception(ErrorCodes::FILE_CACHE_ERROR,
                        "Attempt to reserve space too much space ({}) for file segment with range: {} (downloaded size: {})",
                        size, range().toString(), downloaded_size);

    auto caller_id = getCallerId();
    if (downloader_id != caller_id)
        throw Exception(ErrorCodes::FILE_CACHE_ERROR, "Space can be reserved only by downloader (current: {}, expected: {})", caller_id, downloader_id);

    assert(reserved_size >= downloaded_size);

    std::lock_guard cache_lock(cache->mutex);

    /**
     * It is possible to have downloaded_size < reserved_size when reserve is called
     * in case previous downloader did not fully download current file_segment
     * and the caller is going to continue;
     */
    size_t free_space = reserved_size - downloaded_size;
    size_t size_to_reserve = size - free_space;

    bool reserved = cache->tryReserve(key(), offset(), size_to_reserve, cache_lock);

    if (reserved)
        reserved_size += size;

    return reserved;
}

void FileSegment::completeBatch()
{
    {
        std::lock_guard segment_lock(mutex);

        bool is_downloader = downloader_id == getCallerId();
        if (!is_downloader)
        {
            cv.notify_all();
            throw Exception(ErrorCodes::FILE_CACHE_ERROR, "File segment can be completed only by downloader");
        }

        if (downloaded_size == range().size())
            download_state = State::DOWNLOADED;

        downloader_id.clear();
    }

    cv.notify_all();
}

void FileSegment::complete(State state)
{
    {
        std::lock_guard segment_lock(mutex);

        bool is_downloader = downloader_id == getCallerId();
        if (!is_downloader)
        {
            cv.notify_all();
            throw Exception(ErrorCodes::FILE_CACHE_ERROR,
                            "File segment can be completed only by downloader or downloader's FileSegmentsHodler");
        }

        if (state != State::DOWNLOADED
            && state != State::PARTIALLY_DOWNLOADED
            && state != State::PARTIALLY_DOWNLOADED_NO_CONTINUATION)
        {
            cv.notify_all();
            throw Exception(ErrorCodes::FILE_CACHE_ERROR,
                            "Cannot complete file segment with state: {}", toString(state));
        }

        download_state = state;
        completeImpl(segment_lock);
    }

    cv.notify_all();
}

void FileSegment::complete()
{
    {
        std::lock_guard segment_lock(mutex);

        if (download_state == State::SKIP_CACHE)
            return;

        if (downloaded_size == range().size() && download_state != State::DOWNLOADED)
            download_state = State::DOWNLOADED;

        if (download_state == State::DOWNLOADING || download_state == State::EMPTY)
            download_state = State::PARTIALLY_DOWNLOADED;

        completeImpl(segment_lock);
    }

    cv.notify_all();
}

void FileSegment::completeImpl(std::lock_guard<std::mutex> & /* segment_lock */)
{
    bool download_can_continue = false;

    if (download_state == State::PARTIALLY_DOWNLOADED
                || download_state == State::PARTIALLY_DOWNLOADED_NO_CONTINUATION)
    {
        std::lock_guard cache_lock(cache->mutex);

        bool is_last_holder = cache->isLastFileSegmentHolder(key(), offset(), cache_lock);
        download_can_continue = !is_last_holder && download_state == State::PARTIALLY_DOWNLOADED;

        if (!download_can_continue)
        {
            if (!downloaded_size)
            {
                download_state = State::SKIP_CACHE;
                LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "Remove cell {} (downloaded: {})", range().toString(), downloaded_size);
                cache->remove(key(), offset(), cache_lock);
            }
            else if (is_last_holder)
            {
                /**
                * Only last holder of current file segment can resize the cell,
                * because there is an invariant that file segments returned to users
                * in FileSegmentsHolder represent a contiguous range, so we can resize
                * it only when nobody needs it.
                */
                LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "Resize cell {} to downloaded: {}", range().toString(), downloaded_size);
                cache->reduceSizeToDownloaded(key(), offset(), cache_lock);
            }
        }
    }

    if (downloader_id == getCallerId())
    {
        LOG_TEST(&Poco::Logger::get("kssenii " + range().toString() + " "), "Clearing downloader id: {}, current state: {}", downloader_id, toString(download_state));
        downloader_id.clear();
    }

    if (!download_can_continue && download_buffer)
    {
        download_buffer->sync();
        download_buffer.reset();
    }
}

String FileSegment::toString(FileSegment::State state)
{
    switch (state)
    {
        case FileSegment::State::DOWNLOADED:
            return "DOWNLOADED";
        case FileSegment::State::EMPTY:
            return "EMPTY";
        case FileSegment::State::DOWNLOADING:
            return "DOWNLOADING";
        case FileSegment::State::PARTIALLY_DOWNLOADED:
            return "PARTIALLY DOWNLOADED";
        case FileSegment::State::PARTIALLY_DOWNLOADED_NO_CONTINUATION:
            return "PARTIALLY DOWNLOADED NO CONTINUATION";
        case FileSegment::State::SKIP_CACHE:
            return "SKIP_CACHE";
    }
}

}

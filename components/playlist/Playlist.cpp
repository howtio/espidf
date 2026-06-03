#include "Playlist.hpp"

void Playlist::load(const std::vector<SongInfo>& songs)
{
    songs_ = songs;
    index_ = 0;
}

const SongInfo* Playlist::current() const
{
    if (songs_.empty() || index_ >= songs_.size()) return nullptr;
    return &songs_[index_];
}

const SongInfo* Playlist::next()
{
    if (songs_.empty()) return nullptr;
    index_ = (index_ + 1) % songs_.size();
    return &songs_[index_];
}

const SongInfo* Playlist::prev()
{
    if (songs_.empty()) return nullptr;
    index_ = (index_ == 0) ? songs_.size() - 1 : index_ - 1;
    return &songs_[index_];
}

const SongInfo* Playlist::jump_to(size_t index)
{
    if (index >= songs_.size()) return nullptr;
    index_ = index;
    return &songs_[index_];
}

size_t Playlist::current_index() const { return index_; }
size_t Playlist::total_count() const { return songs_.size(); }
bool Playlist::is_empty() const { return songs_.empty(); }

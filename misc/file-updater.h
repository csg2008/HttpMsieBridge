#pragma once
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;
struct manifest_entry_t;
using manifest_map_t = std::unordered_map<std::string, manifest_entry_t>;


struct manifest_entry_t
{
	std::string hash_sum;
	bool compared_to_local;

	bool remove_at_update;
	bool skip_update;

	manifest_entry_t(std::string& file_hash_sum) : hash_sum(file_hash_sum), compared_to_local(false), remove_at_update(false), skip_update(false){}
};

/* Because Windows doesn't provide us a Unicode
 * command line by default and the command line
 * it does provide us is in UTF-16LE. */
struct MultiByteCommandLine
{
	MultiByteCommandLine();
	~MultiByteCommandLine();

	MultiByteCommandLine(const MultiByteCommandLine&) = delete;
	MultiByteCommandLine(MultiByteCommandLine&&) = delete;
	MultiByteCommandLine &operator=(const MultiByteCommandLine&) = delete;
	MultiByteCommandLine &operator=(MultiByteCommandLine&&) = delete;

	LPSTR *argv() { return m_argv; };
	int argc() { return m_argc; };

private:
	int m_argc{ 0 };
	LPSTR *m_argv{ nullptr };
};

struct FileUpdater
{
	FileUpdater() = delete;
	FileUpdater(FileUpdater&&) = delete;
	FileUpdater(const FileUpdater&) = delete;
	FileUpdater &operator=(const FileUpdater&) = delete;
	FileUpdater &operator=(FileUpdater&&) = delete;

	explicit FileUpdater(fs::path old_files_dir, fs::path app_dir, fs::path new_files_dir, const manifest_map_t & manifest);
	~FileUpdater();

	void update();
	bool update_entry(manifest_map_t::const_iterator  &iter, fs::path & new_files_dir);
	bool update_entry_with_retries(manifest_map_t::const_iterator  &iter, fs::path & new_files_dir);
	void revert();
	bool reset_rights(const fs::path& path);

private:
	fs::path m_old_files_dir;
	fs::path m_app_dir;
	fs::path m_new_files_dir;
	const manifest_map_t & m_manifest;
};

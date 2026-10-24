// WarsBots data only; never relocate the ordinary Crysis profile.
#pragma once
#include <filesystem>
#include <cstdlib>
namespace WarsBots {
inline std::filesystem::path DataDirectory(const wchar_t* child) {
 const wchar_t* local=_wgetenv(L"LOCALAPPDATA");
 std::filesystem::path root=local && *local?std::filesystem::path(local):std::filesystem::temp_directory_path();
 root/=L"WarsBots";root/=child;
 std::error_code ec;std::filesystem::create_directories(root,ec);
 return root;
}
}

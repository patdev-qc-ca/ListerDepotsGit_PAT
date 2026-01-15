#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <iostream>
#pragma comment(lib, "winhttp.lib")

struct InfosDepot {
    std::string fullName;
    std::string url;
    std::string description;
}; 
class Depots {
public:
    void Add(const InfosDepot& repo) {repos.push_back(repo);}
    const std::vector<InfosDepot>& Get() const {return repos;}
    size_t Count() const {return repos.size();}
private:
    std::vector<InfosDepot> repos;
};
#pragma once

class MiniClientGit {
public:
    MiniClientGit(const std::string& pat): pat_(pat) {}
    bool ListerTousLesDepots(Depots& out);
private:
    std::string pat_;
    bool FetchPage(int page, std::string& json, std::string& linkHeader);
    std::vector<InfosDepot> ParseRepos(const std::string& json);
    bool HasNextPage(const std::string& linkHeader);
};

std::wstring s2ws(const std::string& s) {return std::wstring(s.begin(), s.end());}
bool MiniClientGit::FetchPage(int page, std::string& json, std::string& linkHeader) {
    json.clear();
    linkHeader.clear();
    std::wstring host = L"api.github.com";
    std::wstring path = L"/user/repos?per_page=100&page=" + std::to_wstring(page);
    HINTERNET hSession = WinHttpOpen(L"ListerDepotsGit_PAT/1.01",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {WinHttpCloseHandle(hSession);return false;}
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),NULL, WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    std::wstring headers = L"User-Agent: Win32App\r\n";
    headers += L"Accept: application/vnd.github+json\r\n";
    headers += L"Authorization: Bearer " + s2ws(pat_) + L"\r\n";
    if (!WinHttpSendRequest(hRequest, headers.c_str(), -1L,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    WinHttpReceiveResponse(hRequest, NULL);
    DWORD size = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM, L"Link", NULL, &size, WINHTTP_NO_HEADER_INDEX);
    if (size > 0) {
        std::vector<wchar_t> buffer(size / sizeof(wchar_t));
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CUSTOM,L"Link", buffer.data(), &size, WINHTTP_NO_HEADER_INDEX);
        linkHeader = std::string(buffer.begin(), buffer.end());
    }
    do {
        WinHttpQueryDataAvailable(hRequest, &size);
        if (size == 0) break;
        std::vector<char> buf(size+1);
        DWORD downloaded = 0;
        WinHttpReadData(hRequest, buf.data(), size, &downloaded);
        buf[downloaded] = 0;
        json += buf.data();
    } while (size > 0);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}
bool MiniClientGit::HasNextPage(const std::string& linkHeader) {   return linkHeader.find("rel=\"next\"") != std::string::npos;}
std::vector<InfosDepot> MiniClientGit::ParseRepos(const std::string& json) {
    std::vector<InfosDepot> repos;
    size_t pos = 0;
    while ((pos = json.find("\"full_name\":", pos)) != std::string::npos) {
        InfosDepot r;
        size_t start = json.find("\"", pos + 12) + 1;
        size_t end = json.find("\"", start);
        r.fullName = json.substr(start, end - start);
        size_t urlPos = json.find("\"html_url\":", pos);
        if (urlPos != std::string::npos) {
            start = json.find("\"", urlPos + 11) + 1;
            end = json.find("\"", start);
            r.url = json.substr(start, end - start);
        }
        repos.push_back(r);
        pos = end;
    }
    return repos;
}
bool MiniClientGit::ListerTousLesDepots(Depots& out) {
    int page = 1;
    bool more = true;
    while (more) {
        std::string json, link;
        if (!FetchPage(page, json, link))return false;
        auto repos = ParseRepos(json);
        for (auto& r : repos)
            out.Add(r);
        if (repos.size() < 100)break;
        if (!HasNextPage(link))break;
        page++;
    }
    return true;
}
int main() {
    std::wcout << L"ListerDepotsGit_PAT\tv:1.01\t(C)Patrice Waechter-Ebling 2026\n" << std::endl;
    std::string pat;
    std::cout << "PAT GitHub : ";
    std::getline(std::cin, pat);
    MiniClientGit client(pat);
    Depots depots;
    if (!client.ListerTousLesDepots(depots)) {
        std::cout << "Erreur lors de la recuperation.\n";
        return 1;
    }
    std::wcout << depots.Count() <<L" depots trouves\n";
    return 0;
}

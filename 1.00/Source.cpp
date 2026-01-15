#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

std::wstring s2ws(const std::string& s) {return std::wstring(s.begin(), s.end());}

std::vector<std::string> ExtractRepoNames(const std::string& json) {
    std::vector<std::string> repos;
    size_t pos = 0;
    while ((pos = json.find("\"full_name\":", pos)) != std::string::npos) {
        pos = json.find("\"", pos + 12);
        if (pos == std::string::npos) break;
        size_t start = pos + 1;
        size_t end = json.find("\"", start);
        if (end == std::string::npos) break;
        repos.push_back(json.substr(start, end - start));
        pos = end;
    }
    if (repos.empty()) {
        pos = 0;
        while ((pos = json.find("\"name\":", pos)) != std::string::npos) {
            pos = json.find("\"", pos + 7);
            if (pos == std::string::npos) break;
            size_t start = pos + 1;
            size_t end = json.find("\"", start);
            if (end == std::string::npos) break;
            repos.push_back(json.substr(start, end - start));
            pos = end;
        }
    }
    return repos;
}

int main() {
    std::string pat;
    std::cout << "Entrez votre GitHub PAT (Personal Access Token) : ";
    std::getline(std::cin, pat);
    if (pat.empty()) {std::cout << "PAT vide, abandon.\n";return 1;}
    std::wstring host = L"api.github.com";
    std::wstring path = L"/user/repos"; 
    HINTERNET hSession = WinHttpOpen(L"Win32 GitHub Client/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {std::cout << "Erreur WinHttpOpen\n";return 1;}
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {std::cout << "Erreur WinHttpConnect\n";WinHttpCloseHandle(hSession);return 1;}
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),NULL, WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        std::cout << "Erreur lors de l'ouverture\n";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }
    std::wstring headers = L"User-Agent: Win32App\r\n";
    headers += L"Accept: application/vnd.github+json\r\n";
    std::wstring wpat = s2ws(pat);
    headers += L"Authorization: Bearer " + wpat + L"\r\n";
    BOOL sent = WinHttpSendRequest(hRequest,headers.c_str(),(DWORD)-1L,WINHTTP_NO_REQUEST_DATA,0,0,0);
    if (!sent) {
    std::cout << "Erreur dans l'envoi de la requete\n";
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return 1;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        std::cout << "Erreur de reponse recue\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 1;
    }
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest,WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&statusCode,&statusCodeSize,WINHTTP_NO_HEADER_INDEX)) {
        if (statusCode != 200) {
            std::cout << "Statut HTTP: " << statusCode << " (attendu: 200)\n";
        }
    }
    DWORD size = 0;
    std::string response;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &size)) {std::cout << "Erreur,données indisponibles\n";break;}
        if (size == 0) break;
        std::vector<char> buffer(size + 1);
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), size, &downloaded)) {std::cout << "Erreur durant la lecture de donnees\n";break;}
        buffer[downloaded] = 0;
        response += buffer.data();
    } while (size > 0);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    auto repos = ExtractRepoNames(response);
    std::cout << "\n=== Repertoires GitHub (PAT) ===\n";
    for (const auto& r : repos) {
        std::cout << " - " << r << "\n";
    }
    if (repos.empty()) {
        std::cout << "Aucun repository trouvé.\n";
        std::cout << "Vérifie que ton PAT a bien le scope 'repo' ou 'read:user'.\n";
    }
    return 0;
}
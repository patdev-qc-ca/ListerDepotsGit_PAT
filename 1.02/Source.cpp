#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#pragma comment(lib, "odbc32.lib")
#pragma comment(lib, "winhttp.lib")

std::wstring connStr_;
/* script SQL
CREATE TABLE GitHubRepos (
    FullName NVARCHAR(200) PRIMARY KEY,
    Url NVARCHAR(500),
    LastSync DATETIME DEFAULT GETDATE()
);
*/
struct RepoInfo {
    std::string fullName;
    std::string url;
    std::string description;
}; 
class AllDepots {
public:
    void Add(const RepoInfo& repo) {repos.push_back(repo);}
    const std::vector<RepoInfo>& Get() const {return repos;}
    size_t Count() const {return repos.size();}
private:
    std::vector<RepoInfo> repos;
};
#pragma once

class GitHubClient {
public:
    GitHubClient(const std::string& pat): pat_(pat) {}
    bool ListerTousLesDepots(AllDepots& out);
private:
    std::string pat_;
    bool FetchPage(int page, std::string& json, std::string& linkHeader);
    std::vector<RepoInfo> ParseRepos(const std::string& json);
    bool HasNextPage(const std::string& linkHeader);
};

std::wstring s2ws(const std::string& s) {return std::wstring(s.begin(), s.end());}
bool GitHubClient::FetchPage(int page, std::string& json, std::string& linkHeader) {
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
bool GitHubClient::HasNextPage(const std::string& linkHeader) {   return linkHeader.find("rel=\"next\"") != std::string::npos;}
std::vector<RepoInfo> GitHubClient::ParseRepos(const std::string& json) {
    std::vector<RepoInfo> repos;
    size_t pos = 0;
    while ((pos = json.find("\"full_name\":", pos)) != std::string::npos) {
        RepoInfo r;
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
bool GitHubClient::ListerTousLesDepots(AllDepots& out) {
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
void InitialiserSQL(const std::wstring& connStr){
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (!SQL_SUCCEEDED(ret)) {std::wcerr << L"[SqlCache] SQLAllocHandle(ENV) failed\n";hEnv = NULL;return; }
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(ret)) {
        std::wcerr << L"[SqlCache] SQLSetEnvAttr failed\n";
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        hEnv = NULL;
        return;   
    }
    ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    if (!SQL_SUCCEEDED(ret)) {
        std::wcerr << L"[SqlCache] SQLAllocHandle(DBC) failed\n";
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        hEnv = NULL;
        hDbc = NULL;
        return;
    }
    ret = SQLDriverConnectW(hDbc,NULL,(SQLWCHAR*)connStr_.c_str(),SQL_NTS,NULL,0,NULL,SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(ret)) {
        std::wcerr << L"[SqlCache] SQLDriverConnect failed\n";
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        hDbc = NULL;
        hEnv = NULL;
        return;
    }
    std::wcout << L"[SqlCache] Connected to SQL Server successfully\n";
}
bool FonctionSQLExists(const std::string& fullName)
{
    SQLHENV  hEnv = NULL;
    SQLHDBC  hDbc = NULL;
    SQLHSTMT hStmt = NULL;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS)return false;
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }
    SQLRETURN ret = SQLDriverConnectW(hDbc,NULL,(SQLWCHAR*)connStr_.c_str(),SQL_NTS,NULL,0,NULL,SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(ret)) {
        std::cout << "SQLDriverConnect failed\n";
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }
    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) != SQL_SUCCESS) {
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }
    std::wstring sql = L"SELECT COUNT(*) FROM GitHubRepos WHERE FullName = ?";
    ret = SQLPrepareW(hStmt, (SQLWCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cout << "SQLPrepare failed\n";
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }
    std::wstring wFullName(fullName.begin(), fullName.end());
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,wFullName.size(), 0, (SQLPOINTER)wFullName.c_str(), 0, NULL);
    ret = SQLExecute(hStmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cout << "SQLExecute failed\n";
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }
    SQLLEN count = 0;
    ret = SQLFetch(hStmt);
    if (SQL_SUCCEEDED(ret)) {SQLGetData(hStmt, 1, SQL_C_SLONG, &count, 0, NULL);}
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    return (count > 0);
}
bool SauvegarderSQL(const RepoInfo& repo) {
        SQLHENV hEnv = NULL;
        SQLHDBC hDbc = NULL;
        SQLHSTMT hStmt = NULL;
        if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS)return false;
        SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) {SQLFreeHandle(SQL_HANDLE_ENV, hEnv);return false;}
        SQLRETURN ret = SQLDriverConnectW(hDbc,NULL,(SQLWCHAR*)connStr_.c_str(),SQL_NTS,NULL,0,NULL,SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(ret)) {
            std::cout << "Echec lors de la connexion SQL\n";
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }
        if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) != SQL_SUCCESS) {
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }
        std::wstring sql =L"INSERT INTO GitHubRepos (FullName, Url) VALUES (?, ?)";
        ret = SQLPrepareW(hStmt, (SQLWCHAR*)sql.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) {
            std::cout << "Echec de la preparation pour envoi SQL\n";
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }
        std::wstring fullName = std::wstring(repo.fullName.begin(), repo.fullName.end());
        std::wstring url = std::wstring(repo.url.begin(), repo.url.end());
        SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,fullName.size(), 0, (SQLPOINTER)fullName.c_str(), 0, NULL);
        SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,url.size(), 0, (SQLPOINTER)url.c_str(), 0, NULL);
        ret = SQLExecute(hStmt);
        if (!SQL_SUCCEEDED(ret)) {std::cout << "Echec lors de l'insertion possible doublon\n";}
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return SQL_SUCCEEDED(ret);
}
std::vector<std::wstring> ListSqlInstances()
{
    std::vector<std::wstring> instances;
    SQLHENV hEnv = NULL;
    SQLHDBC hDbc = NULL;
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
    SQLWCHAR inConnStr[] = L"DRIVER={SQL Server};SERVER=?;";
    SQLWCHAR outConnStr[4096];
    SQLSMALLINT outLen = 0;
    SQLRETURN ret = SQLBrowseConnectW(hDbc,inConnStr,SQL_NTS,outConnStr,sizeof(outConnStr) / sizeof(SQLWCHAR),&outLen);
    if (ret == SQL_NEED_DATA)
    {
        // SERVER=(local),(localdb)\MSSQLLocalDB,PCNAME\SQLEXPRESS,...
        std::wstring result(outConnStr);
        size_t pos = result.find(L"SERVER=");
        if (pos != std::wstring::npos)
        {
            pos += 7; // skip "SERVER="
            size_t end = result.find(L";", pos);
            std::wstring servers = result.substr(pos, end - pos);
            size_t start = 0;
            while (true)
            {
                size_t comma = servers.find(L",", start);
                if (comma == std::wstring::npos) {instances.push_back(servers.substr(start));break;}
                instances.push_back(servers.substr(start, comma - start));
                start = comma + 1;
            }
        }
    }
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    return instances;
}

int main() {
    std::wcout << L"ListerDepotsGit_PAT\tv:1.02\t(C)Patrice Waechter-Ebling 2026\nRecherche des instances Serveur SQL" << std::endl;
    auto instances = ListSqlInstances();
    bool isSQL = false;
    std::wcout << L"Instances SQL Server détectées :" << std::endl;
    for (auto& inst : instances)
        std::wcout << L" - " << inst << std::endl;
    if (instances.empty()){
        std::wcout << L"Aucune instance trouvée." << std::endl;
        isSQL = false;
    }
    else {
        isSQL = true;
    }
    std::string pat;
    std::cout << "PAT GitHub : ";
    std::getline(std::cin, pat);
    GitHubClient client(pat);
    AllDepots depots;
    if (!client.ListerTousLesDepots(depots)) {std::cout << "Erreur lors de la récupération.\n";return 1;}
    std::wcout << L"Total dépôts : " << depots.Count() << L"\n";
    if (isSQL == true) {
        InitialiserSQL(L"DSN=I9-PATRICE\SQLEXPRESS;Trusted_Connection=Yes;");
        for (auto& r : depots.Get()) {
            if (!FonctionSQLExists(r.fullName))SauvegarderSQL(r); 
        }
    }
    return 0;
}
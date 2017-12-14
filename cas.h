#ifndef CAS_H
#define CAS_H

#include <curl/curl.h>

#include <string>

bool CAS_Register(const std::string& sProjectURL, const std::string& sEmail, const std::string& sPassword, std::string& sError);

class CAS_CURL
{
private:
    CURL* curl;
    std::string sBuffer;
    long http_code;

    static size_t WriteCallBack(void* contents, size_t size, size_t nmemb, void* userp)
    {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

public:
    CAS_CURL()
        : curl(curl_easy_init())
    {
    }

    ~CAS_CURL()
    {
        if (curl)
            curl_easy_cleanup(curl);
    }

    std::string CallURL(const std::string& sProjectURL, const std::string& sArguments, std::string& sError)
    {
        CURLcode Result;

        std::string sCallURL = sProjectURL + sArguments;

        curl_easy_setopt(curl, CURLOPT_URL, sCallURL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sBuffer);

        Result = curl_easy_perform(curl);

        if (Result > 0)
        {
            sError = curl_easy_strerror(Result);

            return "";
        }

        return sBuffer;
    }
};

#endif // CAS_H

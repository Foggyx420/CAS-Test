// This Is Just a POC of the CPID Assignment System.
// This can be modified to work in the dash client in a more formal manner
// This is for testing purposes
// UI Will still need to be made
// This just so far shows the proof that you can infact put a hash in the url through a wallet if this option was done.
//
// ExtractXML and MD5 from the Gridcoin II code
#include "cas.h"

#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <random>
#include <openssl/md5.h>
#include <curl/curl.h>

bool fDebug = true;

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end);
std::string MD5_Credentials(const std::string& sCredentials, std::string& sError);
std::string CAS_Error(const std::string& sErrorType, const std::string& sData);

// Process XML

std::string ExtractXML(const std::string& XMLdata, const std::string& key, const std::string& key_end)
{
    std::string extraction = "";
    std::string::size_type loc = XMLdata.find(key, 0);

    if(loc != std::string::npos)
    {
        std::string::size_type loc_end = XMLdata.find(key_end, loc+3);

        if (loc_end != std::string::npos)
            extraction = XMLdata.substr(loc+(key.length()), loc_end-loc-(key.length()));
    }

    return extraction;
}

// MD5

std::string MD5_Credentials(const std::string& sCredentials, std::string& sError)
{
    try
    {
        const char* MD5Char = sCredentials.c_str();
        unsigned char MD5Digest[MD5_DIGEST_LENGTH];
        char MD5Result[33];

        MD5((unsigned char*)MD5Char, strlen(MD5Char), (unsigned char*)MD5Digest);

        for(int i = 0; i < 16; i++)
            sprintf(&MD5Result[i*2], "%02x", (unsigned int)MD5Digest[i]);

        std::string MD5Return(MD5Result);

        return MD5Return;
    }

    catch (std::exception &e)
    {
        sError = "Exception occured -> %s\n" + std::string(e.what());

        return "";
    }
}

// MD5, CURL, MISC and XML Errors

std::string CAS_Error(const std::string& sErrorType, const std::string& sData)
{
    // Make it formatted. this is not needed but looked neater
    return sErrorType + ": " + sData;
}

int main()
{
    // Variables for testing
    std::string sProjectURL = "http://szdg.lpds.sztaki.hu/szdg/";
    std::string sEmail = "emailhere";
    std::string sPassword = "passwordhere";
    std::string sError;

    if (!CAS_Register(sProjectURL, sEmail, sPassword, sError))
    {
        printf("%s\n", sError.c_str());

        return 0;
    }

    printf("Success\n");

    return 0;
}

bool CAS_Register(const std::string& sProjectURL, const std::string& sEmail, const std::string& sPassword, std::string& sError)
{
    std::locale loc;
    std::string sErrorReturn;
    std::string sMD5Hash;
    std::string sCredentials;
    std::string sCURLReturned;
    std::string sAcctAuth;
    std::string sCPID;
    std::string sUserID;
    std::string sSetHash;
    std::string sVerifyHash;
    std::string sXML;

    // Lower case the email -- Test for other languages
    for (auto element : sEmail)
        element = std::tolower(element, loc);

    sCredentials = sPassword + sEmail;

    // Part 1 -- MD5 Credentials
    sMD5Hash = MD5_Credentials(sCredentials, sErrorReturn);

    if (!sErrorReturn.empty())
    {
        sError = CAS_Error("CAS[MD5]", sErrorReturn);

        return false;
    }

    if (fDebug)
        printf("MD5 is %s\n", sMD5Hash.c_str());

    // Part 2 -- Aquire Account key
    CAS_CURL GetAcctAuth;

    sCURLReturned = GetAcctAuth.CallURL(sProjectURL, "lookup_account.php?email_addr=" + sEmail + "&passwd_hash=" + sMD5Hash, sErrorReturn);

    if (!sErrorReturn.empty())
    {
        sError = CAS_Error("CAS[CURL]", sErrorReturn);

        return false;
    }

    sXML = ExtractXML(sCURLReturned, "<error_msg>", "</error_msg>");

    if (!sXML.empty())
    {
        sError = CAS_Error("CAS[LOOKUPACCOUNT]", sXML);

        return false;
    }

    sAcctAuth = ExtractXML(sCURLReturned, "<authenticator>", "</authenticator>");

    if (sAcctAuth.empty())
    {
        sError = CAS_Error("CAS[LOOKUPACCOUNT]", "Authenticator xml empty");

        return false;
    }

    if (fDebug)
        printf("Acct Auth is %s\n", sAcctAuth.c_str());

    // Part 3 -- Generate hash and set to xml field
    // Generate hex hash lets say 32 characters long (0-9, a-f)
    // I admit seeding with nanoseconds may seem insane but maybe
    // less predictable thou likely not needed but was fun
    try
    {
        const char* hex_digits = "0123456789abcdef";

        std::mt19937_64 Generator;

        auto t = std::chrono::high_resolution_clock::now();
        Generator.seed(t.time_since_epoch().count());
        std::uniform_int_distribution<> Distribution(0,15);

        for (unsigned int h = 0; h < 32; h++)
            sSetHash += hex_digits[Distribution(Generator)];
    }

    catch (std::exception &e)
    {
        sError = CAS_Error("CAS[HASH]", std::string(e.what()));

        return false;
    }

    CAS_CURL SetHash;

    sCURLReturned = SetHash.CallURL(sProjectURL, "am_set_info.php?account_key=" + sAcctAuth + "&url=" + sSetHash, sErrorReturn);

    if (!sErrorReturn.empty())
    {
        sError = CAS_Error("CAS[CURL]", sErrorReturn);

        return false;
    }

    sXML = ExtractXML(sCURLReturned, "<error_msg>", "</error_msg>");

    if (!sXML.empty())
    {
        sError = CAS_Error("CAS[SETINFO]", sXML);

        return false;
    }

    if (sCURLReturned.find("<success/>") == std::string::npos)
    {
        sError = CAS_Error("CAS[SETINFO]", "Call was not successful server side despite no errors");

        return false;
    }

    if (fDebug)
        printf("Set Hash is %s\n", sSetHash.c_str());

    // Part 4 -- Verify set hash and pull User ID
    CAS_CURL VerifyHash;

    sCURLReturned = VerifyHash.CallURL(sProjectURL, "am_get_info.php?account_key=" + sAcctAuth, sErrorReturn);

    if (!sErrorReturn.empty())
    {
        sError = CAS_Error("CAS[CURL]", sErrorReturn);

        return false;
    }

    sXML = ExtractXML(sCURLReturned, "<error_msg>", "</error_msg>");

    if (!sXML.empty())
    {
        sError = CAS_Error("CAS[VERIFYHASH]", sXML);

        return false;
    }

    sVerifyHash = ExtractXML(sCURLReturned, "<url>", "</url>");
    sUserID = ExtractXML(sCURLReturned, "<id>", "</id>");

    if (sVerifyHash.empty() || sUserID.empty())
    {
        sError = CAS_Error("CAS[VERIFYHASH]", "Empty xml field for url or id");

        return false;
    }

    if (sSetHash != sVerifyHash)
    {
        sError = CAS_Error("CAS[HASHVERIFY]", "Hashes do not match -> " + sSetHash + " != " + sVerifyHash);

        return false;
    }

    // Part 5 -- Pull CPID
    CAS_CURL PullCPID;

    sCURLReturned = PullCPID.CallURL(sProjectURL, "show_user.php?userid=" + sUserID + "&format=xml", sErrorReturn);

    if (!sErrorReturn.empty())
    {
        sError = CAS_Error("CAS[CURL]", sErrorReturn);

        return false;
    }

    sXML = ExtractXML(sCURLReturned, "<error_msg>", "</error_msg>");

    if (!sXML.empty())
    {
        sError = CAS_Error("CAS[PULLCPID]", sXML);

        return false;
    }

    sCPID = ExtractXML(sCURLReturned, "cpid>", "</cpid>");

    if (sCPID.empty())
    {
        sError = CAS_Error("CAS[PULLCPID]", "empty xml field for cpid");

        return false;
    }

    return true;
}

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <iomanip>
#include <iostream>
#include <istream>
#include <fstream>
#include <cstring>
#include "Utilities.h"

#define BUF_SIZE 1024
/**
 * Utility function for SHA3-256 hashing
 * @param data - chunk of data to be hashed
 * @return std::string containing the hex representation of the hash
 */
std::string computeHash(const std::vector<char>& data) {
    EVP_MD_CTX *ctx;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    ctx=EVP_MD_CTX_new();
    EVP_MD_CTX_init(ctx);
    EVP_DigestInit(ctx, EVP_sha3_256());
    EVP_DigestUpdate(ctx, data.data(), data.size());

    if (EVP_DigestFinal_ex(ctx, md_value, &md_len)!=1)
        return "";

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    ss<<std::hex<<std::setfill('0');
    for(int i=0; i<md_len; i++)
        ss<<std::setw(2)<<static_cast<unsigned>(md_value[i]);

    return ss.str();
}

/**
 * Utility function for SHA3-256 file hashing
 * @param path - path of the file to be hashed
 * @return std::string containing the hex representation of the file's hash
 */
std::string computeFileHash(const std::string& path){
    EVP_MD_CTX *ctx;
    unsigned char md_value[EVP_MAX_MD_SIZE], buf[BUF_SIZE];
    unsigned int md_len;
    std::ifstream fs;
    fs.open(path,std::ios::in | std::ios::binary);

    if(fs.fail()) {
        std::cout << strerror(errno) << std::endl;
        return "";
    }

    ctx=EVP_MD_CTX_new();
    EVP_MD_CTX_init(ctx);
    EVP_DigestInit(ctx, EVP_sha3_256());

    while(!fs.eof()){
        fs.read((char*)buf,BUF_SIZE);
        EVP_DigestUpdate(ctx,buf,fs.gcount());
    }


    if(EVP_DigestFinal_ex(ctx,md_value,&md_len)!=1)
        return "";

    EVP_MD_CTX_free(ctx);
    fs.close();

    std::stringstream ss;
    ss<<std::hex<<std::setfill('0');
    for(int i=0; i<md_len; i++)
        ss<<std::setw(2)<<static_cast<unsigned>(md_value[i]);

    return ss.str();
}

std::string getActionString(int opcode) {
    switch(opcode){
        case 101: return "create_file";
        case 102: return "create_dir";
        case 103: return "rename_file";
        case 104: return "rename_dir";
        case 105: return "remove_entry";
        case 106: return "login";
        case 107: return "check_file";
        case 108: return "ping";
        case 109: return "check_dir";
        case 110: return "start_probe";
        case 199: return "eop";
        case 200: return "ok";
        case 400: return "error";
        default: return "null";
    }
}

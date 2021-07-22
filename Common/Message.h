/*
 * Maximum path length is set to 260 characters as that is
 * the smallest maximum path length (with no particular configurations)
 * between Windows and Linux systems
 *
 * The maximum body length is set to 1024 Bytes which seems a reasonably size.
 *
 * http://pages.cs.wisc.edu/~remzi/Classes/537/LectureNotes/Papers/windows-fs.pdf
 */

/*
 * MAX_MSG_LEN is needed in order to know the exact (and fixed) number of digits
 * to read in every message in order to read and store it properly.
 * It is set to 4 digits and it is determined by:
 *  - Actual message length to be sent as first part of the message, fixed to
 *  - Max path length (decided as it is stated above)
 *  - Max body length that is the maximum amount of data that can be sent in a single message
 *  - File hash size (fixed), of 256 bytes due to the SHA3-256 hashing algorithm used
 *  - Opcode of fixed 3 digits
 *  - JSON syntax
 *
 * The protocol is meant to send only the aforementioned fields and
 * at most 1024 bytes of body data per message to reduce both workload and
 * resources usage but since the message length will be always wrote/read
 * on 4 digits actually there is space for future modifications
 * and/or newer implementations and additions.
 */

#pragma once
#include <vector>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include "../Utilities/Utilities.h"
#include "../Utilities/base64.h"



#define MAX_MSG_LEN 4
#define MAX_PATH_LEN 260
#define MAX_BODY_LEN 1024

/**
 * eop=end of operation
 */
enum Action{null=0, create_file=101, create_dir=102, rename_file=103, rename_dir=104, remove_entry=105, login=106, check_file=107, ping=108, check_dir=109, start_probe=110, eop=199, ok=200, error=400};

class Message {
    std::size_t msgLen;
    std::string dataHash;
    std::string filePath;
    std::vector<char> fileData;
    bool dataAvailable;
    Action opcode;

public:

    Message();

    Message(Action opc, std::string path,std::vector<char>  data);

    Message(Action opc, std::string path);

    std::string getJSON();

    int parseJSON(const std::vector<char>& jsonVect);

    size_t getMsgLen() const;

    const std::string &getDataHash() const;

    const std::string &getFilePath() const;

    const std::vector<char> &getFileData() const;

    Action getOpcode() const;

    void setFilePath(std::string filePath);

    void setFileData(std::vector<char> fileData);

    void setOpcode(Action opc);

    bool getDataAvailable() const;

};


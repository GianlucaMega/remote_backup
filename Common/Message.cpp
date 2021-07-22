#include "Message.h"


/**
 * @return the length of the JSON string of the message
 */
size_t Message::getMsgLen() const {
    return msgLen;
}

/**
 * @return SHA3-256 digest of the file pointed by filePath (if regular file)
 */
const std::string &Message::getDataHash() const {
    return dataHash;
}

/**
 * @return path of the entry the message refers to
 */
const std::string &Message::getFilePath() const {
    return filePath;
}

/**
 * @return file data chunk sent with the message if any,
 */
const std::vector<char> &Message::getFileData() const {
        return fileData;
}

/**
 * @return opcode of the message
 */
Action Message::getOpcode() const {
    return opcode;
}

/**
 * @return true if the data vector is filled, false otherwise
 */
bool Message::getDataAvailable() const{
    return dataAvailable;
}

/**
 * Default constructor for empty messages
 */
Message::Message(): msgLen(0), dataHash(""), filePath(""), dataAvailable(false), opcode(null) {
}

/**
 * Message constructor with parameters. Automatic computation of SHA3-256 digest of the data passed
 * @param opc - opcode of the message
 * @param path - path of the entry or username on login type messages
 * @param data - data chunk of the file pointed by path (if any) or password on login messages (discarded after digest computation)
 */
Message::Message(Action opc, std::string path, std::vector<char>  data): dataAvailable(true), filePath(std::move(path)), fileData(std::move(data)), opcode(opc), msgLen(0) {
    std::replace( filePath.begin(), filePath.end(), '\\', '/');
    dataHash=computeHash(fileData);

    if (dataHash.empty()) {
        std::cout << "Digest computation problem!\n" << std::endl;
    }

    if(opcode==login){
        dataAvailable=false;
        fileData.clear();
        fileData.shrink_to_fit();
    }


}

/**
 * Message constructor for messages without data
 * @param opc - opcode of the message
 * @param path - path of the entry
 */
Message::Message(Action opc, std::string path): msgLen(0), dataHash(""), filePath(std::move(path)), dataAvailable(false), opcode(opc) {
    std::replace( filePath.begin(), filePath.end(), '\\', '/');
}

/**
 * @return json representation of the message or<br>
 * &nbsp&nbsp&nbsp&nbsp "JSON Error" if json parsing fails <br>
 * &nbsp&nbsp&nbsp&nbsp "Data Error" if property tree field conversion fails <br>
 * &nbsp&nbsp&nbsp&nbsp "Base64 Error" if the base64 encoding of the data chunk fails
 */
 /*
  * We preferred to always send all fields even if they are null
  * because searching whether a field is present or not could be
  * quite computationally expensive when scaling up.
  * On the contrary transferring a small unneeded amount of data
  * certainly is a little waste of memory and network resources
  * but it prevents the software from being computationally heavy
  * and hard to read due to the various controls that would have been needed.
  * Another possible approach could be a precise case-wise field read
  * but then the code would be very messy with no such appreciable benefit.
  *
  * https://www.boost.org/doc/libs/1_74_0/doc/html/property_tree/synopsis.html
  * https://www.boost.org/doc/libs/1_75_0/doc/html/property_tree/accessing.html
  */
std::string Message::getJSON() {
    try {
        std::stringstream jsonstream,lenstream;
        std::string jsonstring="";
        boost::property_tree::ptree pt;
        pt.put("Opcode", int(opcode));
        pt.put("Path", filePath.c_str());
        pt.put("Hash", dataHash.c_str());
        if(dataAvailable)
            pt.put("Data", base64_encode((unsigned char*)fileData.data(), fileData.size()));
        else
            pt.put("Data", "");
        boost::property_tree::json_parser::write_json(jsonstream, pt);
        msgLen=jsonstream.str().size();
        lenstream<<std::setfill(' ')<<std::setw(MAX_MSG_LEN)<<msgLen;
        jsonstring.append(lenstream.str());
        jsonstring.append(jsonstream.str());
        return jsonstring;

    } catch (const boost::property_tree::json_parser_error& exc) {
        std::cout<<"JSON parsing error on file: "<<filePath<<std::endl;
        return "JSON Error";
    } catch (const boost::property_tree::ptree_bad_data& exc) {
        std::cout<<"Ptree field conversion error on file: "<<filePath<<std::endl;
        return "Data Error";
    } catch (const std::runtime_error& exc){
        std::cout<<"Base64 encoding error on file :"<<filePath<<std::endl;
        return "Base64 Error";
    }
}

/**
 * Fills the Message object with fields from the provided JSON
 * @param jsonVect - std::vector<char> containing the JSON string to be parsed
 * @return 0 on success<br>
 * &nbsp&nbsp&nbsp&nbsp -1 in case of parsing error<br>
 * &nbsp&nbsp&nbsp&nbsp -2 if property tree field conversion fails <br>
 * &nbsp&nbsp&nbsp&nbsp -3 if property tree field path is incorrect <br>
 * &nbsp&nbsp&nbsp&nbsp -4 if the base64 encoding of the data chunk fails
 *
 */
int Message::parseJSON(const std::vector<char>& jsonVect) {
    try{
        boost::property_tree::ptree pt;
        std::stringstream sstream(std::string(jsonVect.begin(),jsonVect.end()));
        std::string datatmp;
        int optmp;

        boost::property_tree::json_parser::read_json(sstream,pt);
        msgLen=jsonVect.size();
        filePath=pt.get<std::string>("Path");
        dataHash=pt.get<std::string>("Hash");

        datatmp=pt.get<std::string>("Data");
        if(datatmp.empty()){
            fileData.clear();
            fileData.reserve(0);
        } else{
            datatmp=base64_decode(datatmp);
            fileData.reserve(datatmp.length());
            fileData.assign(datatmp.begin(),datatmp.end());
            fileData.shrink_to_fit();
        }
        dataAvailable= !datatmp.empty();

        optmp=pt.get<int>("Opcode");

        switch(optmp){
            case 101: opcode=create_file; break;
            case 102: opcode=create_dir; break;
            case 103: opcode=rename_file; break;
            case 104: opcode=rename_dir; break;
            case 105: opcode=remove_entry; break;
            case 106: opcode=login; break;
            case 107: opcode=check_file; break;
            case 108: opcode=ping; break;
            case 109: opcode=check_dir; break;
            case 110: opcode=start_probe; break;
            case 199: opcode=eop; break;
            case 200: opcode=ok; break;
            case 400: opcode=error; break;
            default: opcode=null; break;
        }

        return 0;

    }catch (const boost::property_tree::json_parser_error& exc) {
        std::cout<<"JSON parsing error"<<std::endl;
        return -1;
    } catch (const boost::property_tree::ptree_bad_data& exc) {
        std::cout<<"Ptree field conversion error"<<std::endl;
        return -2;
    } catch (const boost::property_tree::ptree_bad_path& exc) {
        std::cout<<"Ptree field path error"<<std::endl;
        return -3;
    } catch (const std::runtime_error& exc){
        std::cout<<"Base64 decoding error"<<std::endl;
        return -4;
    }
}

/**
 * Set the file path
 * @param path - path of the entry
 */
void Message::setFilePath(std::string path) {
    filePath = std::move(path);
    std::replace( filePath.begin(), filePath.end(), '\\', '/');
}

/**
 * Store a piece of data
 * @param data - data chunk to be stored
 */
void Message::setFileData(std::vector<char> data) {
    fileData = std::move(data);
    dataHash=computeHash(fileData);
    dataAvailable=true;

    if (dataHash.empty()) {
        std::cout << "Digest computation problem!\n" << std::endl;
    }

    if(opcode==login){
        fileData.clear();
        fileData.shrink_to_fit();
        dataAvailable=false;
    }

}

/**
 * Set the opcode of the message
 * @param opc - Action enum of the opcode
 */
void Message::setOpcode(Action opc) {
    Message::opcode = opc;
    if(opcode==login && dataAvailable){
        dataAvailable=false;
        fileData.clear();
        fileData.shrink_to_fit();
    }
}
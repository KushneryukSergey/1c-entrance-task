#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>
#include <exception>

using std::string;
using std::tuple;
using std::vector;
using std::exception;
using std::cout;

enum class Mode {
    Diff,
    Restore
};

bool is_option(const char* line) {
    if (strlen(line) != 0 && line[0] == '-') {
        return true;
    }
    return false;
}

Mode parse_option(const char* line) {
    if (strcmp(line, "--diff") == 0) {
        return Mode::Diff;
    }
    if (strcmp(line, "--restore") == 0) {
        return Mode::Restore;
    }
    throw std::runtime_error(string("Wrong option ") + string(line));
}

class Arguments {
private:
    Mode mode_;
    string old_nm_;
    string new_nm_;
    string out_nm_;
public:
    Arguments(int argc, char** argv) {
        size_t argi = 1;
        if (argi >= argc) {
            throw std::runtime_error("No arguments");
        }
        mode_ = is_option(argv[argi]) ? parse_option(argv[argi++]) : Mode::Diff;
        if (argi + 1 >= argc) {
            throw std::runtime_error("Too few arguments");
        }
        old_nm_ = argv[argi++];
        new_nm_ = argv[argi++];
        out_nm_ = argi < argc ? argv[argi] : "a.out";
    }
    
    const string& GetOld() {
        return old_nm_;
    }
    
    const string& GetNew() {
        return new_nm_;
    }
    
    const string& GetOut() {
        return out_nm_;
    }
    
    Mode GetMode() {
        return mode_;
    }
};

static const size_t BUFFER_SIZE = 4096;

class IfstreamHangler {
private:
    std::ifstream stream_;
    uint8_t* buf_;
    size_t sz_;
    size_t done_ = 0;
    size_t ind_ = 0;
    
public:
    explicit IfstreamHangler(const string& name):
            stream_(name, std::ios::binary),
            buf_(new uint8_t[BUFFER_SIZE]),
            ind_(BUFFER_SIZE) {
        if (stream_.fail()) {
            throw std::runtime_error("Error while handling files");
        }
        stream_.seekg(0, std::ifstream::end);
        sz_ = stream_.tellg();
        stream_.seekg(0, std::ifstream::beg);
    }
    
    bool IsEof() const {
        return done_ == sz_;
    }
    
    uint8_t GetByte() {
        if (ind_ == BUFFER_SIZE) {
            ind_ = 0;
            stream_.seekg(done_);
            stream_.read((char*) buf_, std::min(sz_ - done_, BUFFER_SIZE));
        }
        ++done_;
        return buf_[ind_++];
    }
    
    ~IfstreamHangler() {
        delete[] buf_;
        stream_.close();
    }
};


class OfstreamHangler {
private:
    std::ofstream stream_;
    uint8_t* buf_;
    size_t ind_ = 0;

public:
    explicit OfstreamHangler(const string& name) :
            stream_(name, std::ios::binary),
            buf_(new uint8_t[BUFFER_SIZE]) {
        if (stream_.fail()) {
            throw std::runtime_error("Error while handling files");
        }
    }
    
    void PutByte(const uint8_t& byte) {
        cout << "Putting byte" << std::endl;
        if (ind_ == BUFFER_SIZE) {
            ind_ = 0;
            stream_.write((char*) buf_, BUFFER_SIZE);
        }
        buf_[ind_++] = byte;
    }
    
    ~OfstreamHangler() {
        if (ind_) {
            stream_.write((char*) buf_, ind_);
        }
        delete[] buf_;
        stream_.close();
    }
};


void Diff(IfstreamHangler& old_file, IfstreamHangler& new_file, std::ofstream& out_file) {
    uint8_t old_byte, new_byte;
    size_t offset = 0;
    while (!old_file.IsEof() && !new_file.IsEof()) {
        old_byte = old_file.GetByte();
        new_byte = new_file.GetByte();
        std::cerr << (int) old_byte << ' ' << (int) new_byte << '\n';
        if (old_byte != new_byte) {
            out_file << offset << " c "
                     << (int) old_byte << ' ' << (int) new_byte << '\n';
        }
        ++offset;
    }
    while (!old_file.IsEof()) {
        old_byte = old_file.GetByte();
        out_file << offset++ << " d\n";
    }
    while (!old_file.IsEof()) {
        new_byte = new_file.GetByte();
        out_file << offset++ << " i\n";
    }
    out_file.close();
}


void Restore(IfstreamHangler& old_file, std::ifstream& in_file, OfstreamHangler& new_file) {
    uint8_t old_byte, new_byte;
    size_t offset = 0;
    size_t old_offset = 0;
    string type;
    int byte;
    while (!in_file.eof()) {
        in_file >> offset >> type;
        while (!old_file.IsEof() && old_offset < offset) {
            ++old_offset;
            new_file.PutByte(old_file.GetByte());
        }
        if (type == "c") {
            old_file.GetByte();
            in_file >> byte >> byte;
            new_file.PutByte(byte);
        } else if (type == "d") {
            continue;
        } else if (type == "i") {
            in_file >> byte;
            new_file.PutByte(byte);
        } else {
            throw std::runtime_error("Wrong symbol for diff type");
        }
    }
    while (!old_file.IsEof()) {
        ++old_offset;
        new_file.PutByte(old_file.GetByte());
    }
    in_file.close();
}


int main(int argc, char** argv) {
    try {
        Arguments args(argc, argv);
    
        if (args.GetMode() == Mode::Diff) {
            IfstreamHangler old_file_diff(args.GetOld());
            IfstreamHangler new_file_diff(args.GetNew());
            std::ofstream out_file(args.GetOut(), std::ios::out);
            Diff(old_file_diff, new_file_diff, out_file);
        } else if (args.GetMode() == Mode::Restore) {
            IfstreamHangler old_file_restore(args.GetOld());
            std::ifstream diff_file_restore(args.GetNew());
            OfstreamHangler new_file_restore(args.GetOut());
            Restore(old_file_restore, diff_file_restore, new_file_restore);
        }
    } catch (const exception& e) {
        cout << "Error occured during work: " << e.what() << std::endl;
        return 0;
    }
}

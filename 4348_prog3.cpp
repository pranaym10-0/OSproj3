#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#define BLOCK_SIZE 512
#define MAGIC_NUMBER "4337PRJ3"
#define MAX_KEYS 19
#define MAX_CHILDREN 20

using namespace std;

uint64_t to_big_endian(uint64_t value) {
    uint8_t dest[8];
    for (int i = 0; i < 8; ++i)
        dest[i] = (value >> ((7 - i) * 8)) & 0xFF;
    return *reinterpret_cast<uint64_t*>(dest);
}

uint64_t from_big_endian(uint64_t value) {
    uint8_t* src = reinterpret_cast<uint8_t*>(&value);
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i)
        result |= (uint64_t(src[i]) << ((7 - i) * 8));
    return result;
}

// Node
struct BTreeNode {
    uint64_t block_id = 0;
    uint64_t parent_id = 0;
    uint64_t num_keys = 0;
    uint64_t keys[MAX_KEYS] = {0};
    uint64_t values[MAX_KEYS] = {0};
    uint64_t children[MAX_CHILDREN] = {0};

    void serialize(char* buffer) {
        memcpy(buffer, &block_id, 8);
        memcpy(buffer + 8, &parent_id, 8);
        memcpy(buffer + 16, &num_keys, 8);

        int offset = 24;
        for (int i = 0; i < MAX_KEYS; i++) {
            memcpy(buffer + offset, &keys[i], 8);
            offset += 8;
        }
        for (int i = 0; i < MAX_KEYS; i++) {
            memcpy(buffer + offset, &values[i], 8);
            offset += 8;
        }
        for (int i = 0; i < MAX_CHILDREN; i++) {
            memcpy(buffer + offset, &children[i], 8);
            offset += 8;
        }
    }

    static BTreeNode deserialize(const char* buffer) {
        BTreeNode node;
        memcpy(&node.block_id, buffer, 8);
        memcpy(&node.parent_id, buffer + 8, 8);
        memcpy(&node.num_keys, buffer + 16, 8);

        int offset = 24;
        for (int i = 0; i < MAX_KEYS; i++) {
            memcpy(&node.keys[i], buffer + offset, 8);
            offset += 8;
        }
        for (int i = 0; i < MAX_KEYS; i++) {
            memcpy(&node.values[i], buffer + offset, 8);
            offset += 8;
        }
        for (int i = 0; i < MAX_CHILDREN; i++) {
            memcpy(&node.children[i], buffer + offset, 8);
            offset += 8;
        }
        return node;
    }
};

// Header
struct BTreeHeader {
    char magic_number[8];
    uint64_t root_id = 0;
    uint64_t next_block_id = 1;

    BTreeHeader() {
        memcpy(magic_number, MAGIC_NUMBER, 8);
    }

    void serialize(char* buffer) {
        memcpy(buffer, magic_number, 8);
        memcpy(buffer + 8, &root_id, 8);
        memcpy(buffer + 16, &next_block_id, 8);
        memset(buffer + 24, 0, BLOCK_SIZE - 24);
    }

    static BTreeHeader deserialize(const char* buffer) {
        BTreeHeader header;
        memcpy(header.magic_number, buffer, 8);
        memcpy(&header.root_id, buffer + 8, 8);
        memcpy(&header.next_block_id, buffer + 16, 8);
        return header;
    }
};

// Index File Manager
class IndexFile {
    fstream file;
    BTreeHeader header;

public:
    void create(const string& filename) {
        file.open(filename, ios::out | ios::binary);
        if (!file.is_open()) {
            cout << "Error creating file.\n";
            return;
        }

        header = BTreeHeader();
        char buffer[BLOCK_SIZE] = {0};
        header.serialize(buffer);
        file.write(buffer, BLOCK_SIZE);

        // Create an empty root node
        BTreeNode root;
        root.block_id = 1;
        root.serialize(buffer);
        file.write(buffer, BLOCK_SIZE);
        file.close();

        cout << "Created index file: " << filename << endl;
    }

    void open(const string& filename) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file.is_open()) {
            cout << "Error opening file.\n";
            return;
        }

        char buffer[BLOCK_SIZE];
        file.read(buffer, BLOCK_SIZE);
        header = BTreeHeader::deserialize(buffer);

        if (strncmp(header.magic_number, MAGIC_NUMBER, 8) != 0) {
            cout << "Invalid magic number.\n";
            file.close();
            return;
        }

        cout << "Opened index file: " << filename << endl;
    }

    void close() {
        if (file.is_open())
            file.close();
    }

    void insert(uint64_t key, uint64_t value) {
        if (!file.is_open()) {
            cout << "No file open.\n";
            return;
        }

        // Load the root node
        char buffer[BLOCK_SIZE];
        file.seekg(BLOCK_SIZE);
        file.read(buffer, BLOCK_SIZE);
        BTreeNode root = BTreeNode::deserialize(buffer);

        // Check for space in the root node
        if (root.num_keys < MAX_KEYS) {
            root.keys[root.num_keys] = key;
            root.values[root.num_keys] = value;
            root.num_keys++;

            // Sort keys and values
            vector<pair<uint64_t, uint64_t>> kv_pairs;
            for (uint64_t i = 0; i < root.num_keys; i++)
                kv_pairs.emplace_back(root.keys[i], root.values[i]);

            sort(kv_pairs.begin(), kv_pairs.end());
            for (uint64_t i = 0; i < root.num_keys; i++) {
                root.keys[i] = kv_pairs[i].first;
                root.values[i] = kv_pairs[i].second;
            }

            // Write back the root node
            file.seekp(BLOCK_SIZE);
            root.serialize(buffer);
            file.write(buffer, BLOCK_SIZE);
            cout << "Inserted key=" << key << ", value=" << value << endl;
        } else {
            cout << "Node is full. Splitting not yet implemented.\n";
        }
    }

    void search(uint64_t key) {
        if (!file.is_open()) {
            cout << "No file open.\n";
            return;
        }

        char buffer[BLOCK_SIZE];
        file.seekg(BLOCK_SIZE);
        file.read(buffer, BLOCK_SIZE);
        BTreeNode root = BTreeNode::deserialize(buffer);

        for (uint64_t i = 0; i < root.num_keys; i++) {
            if (root.keys[i] == key) {
                cout << "Found key=" << key << ", value=" << root.values[i] << endl;
                return;
            }
        }
        cout << "Key not found.\n";
    }

    void load(const string& filename) {
        if (!file.is_open()) {
            cout << "No file open.\n";
            return;
        }

        ifstream input(filename);
        if (!input.is_open()) {
            cout << "Error opening input file.\n";
            return;
        }

        uint64_t key, value;
        char comma;
        while (input >> key >> comma >> value) {
            if (comma != ',') {
                cout << "Invalid format in input file.\n";
                continue;
            }
            insert(key, value);
        }
        cout << "Loaded data from " << filename << endl;
    }

    void print() {
        if (!file.is_open()) {
            cout << "No file open.\n";
            return;
        }

        char buffer[BLOCK_SIZE];
        file.seekg(BLOCK_SIZE);
        file.read(buffer, BLOCK_SIZE);
        BTreeNode root = BTreeNode::deserialize(buffer);

        cout << "Index contents:\n";
        for (uint64_t i = 0; i < root.num_keys; i++) {
            cout << "Key=" << root.keys[i] << ", Value=" << root.values[i] << endl;
        }
    }

    void extract(const string& filename) {
        if (!file.is_open()) {
            cout << "No file open.\n";
            return;
        }

        ofstream output(filename);
        if (!output.is_open()) {
            cout << "Error opening output file.\n";
            return;
        }

        char buffer[BLOCK_SIZE];
        file.seekg(BLOCK_SIZE);
        file.read(buffer, BLOCK_SIZE);
        BTreeNode root = BTreeNode::deserialize(buffer);

        for (uint64_t i = 0; i < root.num_keys; i++) {
            output << root.keys[i] << "," << root.values[i] << "\n";
        }
        cout << "Extracted key-value pairs to " << filename << endl;
    }
};

// Main Menu
int main() {
    IndexFile indexFile;
    string command;

    while (true) {
        cout << "\nCommands: create, open, insert, search, load, print, extract, quit\n";
        cout << "Enter command: ";
        cin >> command;

        if (command == "create") {
            string filename;
            cout << "Enter file name: ";
            cin >> filename;
            indexFile.create(filename);
        } else if (command == "open") {
            string filename;
            cout << "Enter file name: ";
            cin >> filename;
            indexFile.open(filename);
        } else if (command == "insert") {
            uint64_t key, value;
            cout << "Enter key: ";
            cin >> key;
            cout << "Enter value: ";
            cin >> value;
            indexFile.insert(key, value);
        } else if (command == "search") {
            uint64_t key;
            cout << "Enter key: ";
            cin >> key;
            indexFile.search(key);
        } else if (command == "load") {
            string filename;
            cout << "Enter file name to load from: ";
            cin >> filename;
            indexFile.load(filename);
        } else if (command == "print") {
            indexFile.print();
        } else if (command == "extract") {
            string filename;
            cout << "Enter file name to extract to: ";
            cin >> filename;
            indexFile.extract(filename);
        } else if (command == "quit") {
            indexFile.close();
            cout << "Goodbye!\n";
            break;
        } else {
            cout << "Unknown command.\n";
        }
    }

    return 0;
}

#include "config/Config.hpp"
#include "config/parser/ConfigParseError.hpp"
#include "config/parser/ConfigParser.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

#define ASSERT_TRUE(expr) assert((expr) && #expr)
#define ASSERT_EQ(a, b) assert((a) == (b) && "Assertion failed: " #a " == " #b)

static std::string slurpFile(const std::string& path) {
    std::ifstream in(path);
    assert(in && "Failed to open config file");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void test_parse_valid_config(const std::string& path) {
    std::cout << "🧪 Parsing: " << path << "\n";
    try {
        std::string  input = slurpFile(path);
        ConfigParser parser(input);
        Config       config = parser.parseConfig();

        ASSERT_TRUE(!config.getServers().empty());
        for (const Server& s : config.getServers()) {
            int port = s.getPort();
            ASSERT_TRUE(port >= 0 && port <= 65535);
        }

        std::cout << "✅ Parsed OK: " << path << "\n";
    } catch (const ConfigParseError& e) {
        std::cerr << "❌ Unexpected parse error: " << e.what() << "\n";
        assert(false);
    }
}

static void test_parse_invalid_config(const std::string& path) {
    std::cout << "🧪 Parsing (expect fail): " << path << "\n";
    try {
        std::string  input = slurpFile(path);
        ConfigParser parser(input);
        (void) parser.parseConfig();
        std::cerr << "❌ Expected failure but succeeded: " << path << "\n";
        assert(false);
    } catch (const ConfigParseError& e) {
        std::cout << "✅ Correctly failed: " << e.what() << "\n";
    }
}

void test_batch_configs() {
    for (const auto& entry : fs::directory_iterator("./configs/configParser_test")) {
        if (!entry.is_regular_file())
            continue;
        const std::string path  = entry.path().string();
        std::string       input = slurpFile(path);

        if (input.find("# expect-fail") != std::string::npos)
            test_parse_invalid_config(path);
        else
            test_parse_valid_config(path);
    }
}

int main() {
    test_batch_configs();
    std::cout << "✅ All ConfigParser tests passed.\n";
    return 0;
}

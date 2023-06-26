#include "html.hpp"

#include "markdown.hpp"

#include <crow/mustache.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

void set_html_header(crow::response& response) {
    response.add_header("Access-Control-Allow-Origin", "*");
    response.add_header(
        "Content-Security-Policy", "script-src 'self' 'unsafe-eval' 'unsafe-inline'; "
                                   "default-src 'self' 'unsafe-inline' *.corentin.net");
    response.add_header("Strict-Transport-Security", "max-age=63072000");
    response.add_header("X-Frame-Options", "DENY");
    response.add_header("X-Content-Type-Options", "nosniff");
}

crow::response serve_file(const std::string& filename) {
    if (filename.empty()) {
        throw std::runtime_error("cannot serve './' as file");
    }

    std::string adjusted_filename = filename;
    if (adjusted_filename[0] == '/') {
        adjusted_filename = adjusted_filename.substr(1);
    }

    crow::response response;
    response.set_static_file_info(filename);
    if (response.get_header_value("Content-Type") == "text/html") {
        // HTML page
        set_html_header(response);
    } else {
        // Image/script/etc
        response.add_header("Access-Control-Allow-Origin", "*");
    }

    return response;
}

namespace {
bool is_within_tree(const std::string& filename, const fs::path& root) {
    // https://stackoverflow.com/a/67145995/1565581
    std::string relative = std::filesystem::relative(filename, root);
    return relative.size() == 1 || relative[0] != '.' && relative[1] != '.';
}

std::string locate_neighbor_file(const std::string& basepath, const std::string& filename) {
    const auto root = fs::current_path();

    // We only accept paths within the tree, to avoid picking files from the system.
    if (!is_within_tree(basepath, root)) {
        return "";
    }

    auto directory = fs::path(basepath).parent_path();

    // Define a maximum number of iterations to make sure we aren't going to get stuck in a loop.
    constexpr std::size_t max_iter = 10;
    for (std::size_t iter = 0; iter < max_iter; ++iter) {
        if (directory.empty()) {
            directory = root;
        }

        // Look for 'filename' in the directory.
        for (auto const& dir_entry : fs::directory_iterator(directory)) {
            if (dir_entry.path().filename() == filename) {
                return dir_entry.path().string();
            }
        }

        if (!fs::equivalent(directory, root)) {
            // Go back up the tree and try there.
            directory = directory.parent_path();
        } else {
            // No match found within the tree, stop.
            std::cerr << "no " << filename << " found next to " << basepath << std::endl;
            return "";
        }
    }

    std::cerr << "maximum iterations reached trying to locate " << filename << " next to "
              << basepath << std::endl;

    return "";
}

std::size_t get_file_length(std::ifstream& file) {
    const auto ppos = file.tellg();
    file.seekg(0, std::ios::beg);
    const auto spos = file.tellg();
    file.seekg(0, std::ios::end);
    const auto epos = file.tellg();
    file.seekg(ppos, std::ios::beg);

    return epos - spos;
}

std::string read_file(const std::string& filepath) {
    std::string data;

    std::ifstream file(filepath, std::ios::binary);
    data.resize(get_file_length(file));
    file.read(data.data(), data.size());

    return data;
}

std::string read_neighbor_file(const std::string& basepath, const std::string& filename) {
    std::string filepath = locate_neighbor_file(basepath, filename);
    if (filepath.empty()) {
        return "";
    }
    return read_file(filepath);
}
} // namespace

crow::response serve_markdown(const std::string& filename, const crow::json::wvalue& ctx) {
    crow::response response;
    response.code = crow::status::OK;
    response.add_header("Content-Type", "text/html");
    set_html_header(response);

    response.body += read_neighbor_file(filename, "header.html");
    response.body += markdown2html(read_file(filename));
    response.body += read_neighbor_file(filename, "footer.html");

    response.body = crow::mustache::compile(response.body).render_string(ctx);

    return response;
}

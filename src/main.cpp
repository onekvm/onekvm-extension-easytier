#include <json-c/json.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kExtensionID = "easytier";
constexpr std::string_view kDefaultPeers = "tcp://public.easytier.cn:11010";

struct Settings {
  bool service_enabled = true;
  std::string network_name;
  std::string network_secret;
  std::string peers = std::string(kDefaultPeers);
  std::vector<std::string> peer_uris;
  bool dhcp = true;
  std::string ipv4;
  std::string hostname = "onekvm";
  int listen_port = 11010;
};

std::string EnvironmentPath(const char *name, std::string_view fallback) {
  const char *value = std::getenv(name);
  return value != nullptr && *value != '\0' ? value : std::string(fallback);
}

std::string ConfigPath() {
  return EnvironmentPath("ONEKVM_EXTENSION_CONFIG",
                         "/run/onekvm/extensions/easytier") +
         "/config.json";
}

std::string DataPath() {
  return EnvironmentPath("ONEKVM_EXTENSION_DATA",
                         "/data/onekvm/extensions/easytier");
}

bool ReadFile(const std::string &path, bool allow_missing, std::string &data,
              std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (allow_missing && errno == ENOENT)
      return true;
    error = "open " + path + ": " + std::strerror(errno);
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  if (data.size() > (1U << 20)) {
    error = path + " exceeds 1 MiB";
    return false;
  }
  return true;
}

json_object *ParseJSON(std::string_view input, std::string &error) {
  json_tokener *tokener = json_tokener_new_ex(32);
  if (tokener == nullptr) {
    error = "allocate JSON parser";
    return nullptr;
  }
  json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
  json_object *value = json_tokener_parse_ex(
      tokener, input.data(), static_cast<int>(input.size()));
  const auto status = json_tokener_get_error(tokener);
  std::size_t end = json_tokener_get_parse_end(tokener);
  json_tokener_free(tokener);
  while (end < input.size() &&
         std::isspace(static_cast<unsigned char>(input[end])))
    ++end;
  if (status != json_tokener_success || value == nullptr ||
      end != input.size()) {
    if (value != nullptr)
      json_object_put(value);
    error = "invalid JSON";
    return nullptr;
  }
  return value;
}

json_object *ReadCinJSON(std::string &error) {
  json_tokener *tokener = json_tokener_new_ex(32);
  if (tokener == nullptr) {
    error = "allocate JSON parser";
    return nullptr;
  }
  json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
  char buf[4096];
  json_object *value = nullptr;
  json_tokener_error status = json_tokener_continue;
  while (status == json_tokener_continue) {
    std::cin.read(buf, sizeof(buf));
    const std::streamsize n = std::cin.gcount();
    if (n <= 0)
      break;
    value = json_tokener_parse_ex(tokener, buf, static_cast<int>(n));
    status = json_tokener_get_error(tokener);
  }
  json_tokener_free(tokener);
  if (status != json_tokener_success || value == nullptr) {
    if (value != nullptr)
      json_object_put(value);
    error = "invalid JSON";
    return nullptr;
  }
  return value;
}

bool OptionalString(json_object *root, const char *name, std::string &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_string))
    return false;
  output = json_object_get_string(value);
  return true;
}

bool OptionalBool(json_object *root, const char *name, bool &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_boolean))
    return false;
  output = json_object_get_boolean(value) != 0;
  return true;
}

bool OptionalInt(json_object *root, const char *name, int &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_int))
    return false;
  output = json_object_get_int(value);
  return true;
}

bool ValidName(std::string_view name, std::size_t max_length) {
  if (name.empty() || name.size() > max_length)
    return false;
  if (!std::isalnum(static_cast<unsigned char>(name.front())))
    return false;
  for (unsigned char character : name) {
    if (!std::isalnum(character) && character != '-' && character != '_' &&
        character != '.')
      return false;
  }
  return true;
}

bool ValidIPv4(std::string_view value) {
  if (value.empty() || value.size() > 18)
    return false;
  int octets[4] = {0, 0, 0, 0};
  int prefix = -1;
  const int matched =
      std::sscanf(std::string(value).c_str(), "%d.%d.%d.%d/%d", &octets[0],
                  &octets[1], &octets[2], &octets[3], &prefix);
  if (matched != 4 && matched != 5)
    return false;
  for (int octet : octets) {
    if (octet < 0 || octet > 255)
      return false;
  }
  return matched == 4 || (prefix >= 0 && prefix <= 32);
}

bool ValidPeer(std::string_view uri) {
  if (uri.size() < 8 || uri.size() > 256)
    return false;
  const auto scheme_end = uri.find("://");
  if (scheme_end == std::string_view::npos || scheme_end == 0)
    return false;
  const std::string_view scheme = uri.substr(0, scheme_end);
  if (scheme != "tcp" && scheme != "udp" && scheme != "ws" && scheme != "wss" &&
      scheme != "wg")
    return false;
  const std::string_view rest = uri.substr(scheme_end + 3);
  if (rest.empty() || rest.find(' ') != std::string_view::npos)
    return false;
  const auto colon = rest.rfind(':');
  if (colon == std::string_view::npos || colon == 0 || colon + 1 == rest.size())
    return false;
  int port = 0;
  for (unsigned char character : rest.substr(colon + 1)) {
    if (!std::isdigit(character))
      return false;
    port = port * 10 + (character - '0');
    if (port > 65535)
      return false;
  }
  return port >= 1;
}

bool ParsePeers(const std::string &input, std::vector<std::string> &peers,
                std::string &error) {
  peers.clear();
  std::string current;
  auto flush = [&]() {
    if (current.empty())
      return true;
    if (!ValidPeer(current)) {
      error = "invalid EasyTier peer URI";
      return false;
    }
    if (peers.size() >= 8) {
      error = "at most 8 shared nodes are allowed";
      return false;
    }
    peers.push_back(current);
    current.clear();
    return true;
  };
  for (unsigned char character : input) {
    if (character == ',' || character == ';' ||
        std::isspace(character) != 0) {
      if (!flush())
        return false;
      continue;
    }
    current.push_back(static_cast<char>(character));
  }
  return flush();
}

bool LoadSettings(bool allow_missing, Settings &settings, std::string &error) {
  std::string input;
  if (!ReadFile(ConfigPath(), allow_missing, input, error))
    return false;
  if (input.empty())
    return ParsePeers(settings.peers, settings.peer_uris, error);
  json_object *root = ParseJSON(input, error);
  if (root == nullptr)
    return false;
  const bool valid =
      json_object_is_type(root, json_type_object) &&
      OptionalBool(root, "service_enabled", settings.service_enabled) &&
      OptionalString(root, "network_name", settings.network_name) &&
      OptionalString(root, "network_secret", settings.network_secret) &&
      OptionalString(root, "peers", settings.peers) &&
      OptionalBool(root, "dhcp", settings.dhcp) &&
      OptionalString(root, "ipv4", settings.ipv4) &&
      OptionalString(root, "hostname", settings.hostname) &&
      OptionalInt(root, "listen_port", settings.listen_port);
  json_object_put(root);
  if (!valid || settings.network_name.size() > 64 ||
      settings.network_secret.size() > 4096 || settings.peers.size() > 1024 ||
      settings.ipv4.size() > 18 || settings.hostname.size() > 253 ||
      settings.listen_port < 1 || settings.listen_port > 65535 ||
      (!settings.hostname.empty() &&
       !ValidName(settings.hostname, 253)) ||
      (!settings.ipv4.empty() && !ValidIPv4(settings.ipv4))) {
    error = "invalid EasyTier settings";
    return false;
  }
  return ParsePeers(settings.peers, settings.peer_uris, error);
}

bool Ready(const Settings &settings) {
  if (!settings.service_enabled || settings.network_name.empty() ||
      settings.network_secret.empty() || settings.peer_uris.empty())
    return false;
  if (!settings.dhcp && settings.ipv4.empty())
    return false;
  return true;
}

std::string Executable(std::string_view name) {
  std::vector<char> buffer(4096);
  const ssize_t length =
      readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (length <= 0)
    return "bin/" + std::string(name);
  buffer[static_cast<std::size_t>(length)] = '\0';
  return (std::filesystem::path(buffer.data()).parent_path() / name).string();
}

[[noreturn]] void Exec(std::vector<std::string> arguments) {
  std::vector<char *> values;
  values.reserve(arguments.size() + 1);
  for (std::string &argument : arguments)
    values.push_back(argument.data());
  values.push_back(nullptr);
  execv(values.front(), values.data());
  std::cerr << "onekvm-easytier-controller: exec " << arguments.front() << ": "
            << std::strerror(errno) << '\n';
  std::_Exit(127);
}

std::string EscapeTOML(std::string_view input) {
  std::string output = "\"";
  for (unsigned char character : input) {
    switch (character) {
    case '\\':
      output += "\\\\";
      break;
    case '"':
      output += "\\\"";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (character < 0x20) {
        char encoded[8];
        std::snprintf(encoded, sizeof(encoded), "\\u%04x", character);
        output += encoded;
      } else {
        output.push_back(static_cast<char>(character));
      }
      break;
    }
  }
  output += '"';
  return output;
}

bool WriteSecret(const std::string &path, const std::string &data,
                 std::string &error) {
  const int fd =
      open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) {
    error = "open " + path + ": " + std::strerror(errno);
    return false;
  }
  std::string_view remaining(data);
  while (!remaining.empty()) {
    const ssize_t written = write(fd, remaining.data(), remaining.size());
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0) {
      error = "write " + path + ": " + std::strerror(errno);
      close(fd);
      return false;
    }
    remaining.remove_prefix(static_cast<std::size_t>(written));
  }
  if (close(fd) != 0) {
    error = "close " + path + ": " + std::strerror(errno);
    return false;
  }
  return true;
}

std::string RenderConfig(const Settings &settings) {
  std::string output;
  output += "instance_name = \"onekvm\"\n";
  if (!settings.hostname.empty())
    output += "hostname = " + EscapeTOML(settings.hostname) + "\n";
  output += settings.dhcp ? "dhcp = true\n" : "dhcp = false\n";
  if (!settings.dhcp && !settings.ipv4.empty())
    output += "ipv4 = " + EscapeTOML(settings.ipv4) + "\n";
  output += "listeners = [ \"tcp://0.0.0.0:" +
            std::to_string(settings.listen_port) + "\", \"udp://0.0.0.0:" +
            std::to_string(settings.listen_port) + "\" ]\n";
  output += "\n[network_identity]\n";
  output += "network_name = " + EscapeTOML(settings.network_name) + "\n";
  output += "network_secret = " + EscapeTOML(settings.network_secret) + "\n";
  for (const std::string &peer : settings.peer_uris) {
    output += "\n[[peer]]\n";
    output += "uri = " + EscapeTOML(peer) + "\n";
  }
  output += "\n[console_logger]\n";
  output += "level = \"info\"\n";
  return output;
}

void PrintPlan(json_object *response, bool start) {
  json_object *plan = json_object_new_object();
  json_object *actions = json_object_new_array();
  if (start) {
    json_object *ensure = json_object_new_object();
    json_object_object_add(ensure, "type", json_object_new_string("ensure"));
    json_object_object_add(ensure, "name",
                           json_object_new_string("easytier_core"));
    json_object *command = json_object_new_array();
    json_object_array_add(
        command, json_object_new_string("bin/onekvm-easytier-controller"));
    json_object_array_add(command, json_object_new_string("--run"));
    json_object_object_add(ensure, "command", command);
    json_object_object_add(ensure, "restart",
                           json_object_new_string("on-failure"));
    json_object_array_add(actions, ensure);
  } else {
    json_object *remove = json_object_new_object();
    json_object_object_add(remove, "type",
                           json_object_new_string("remove_all"));
    json_object_array_add(actions, remove);
  }
  json_object_object_add(plan, "actions", actions);
  if (response != nullptr)
    json_object_object_add(plan, "response", response);
  std::cout << json_object_to_json_string_ext(plan, JSON_C_TO_STRING_PLAIN)
            << '\n'
            << std::flush;
  json_object_put(plan);
}

void PrintReplacePlan() {
  json_object *plan = json_object_new_object();
  json_object *actions = json_object_new_array();
  json_object *remove = json_object_new_object();
  json_object_object_add(remove, "type", json_object_new_string("remove_all"));
  json_object_array_add(actions, remove);
  json_object *ensure = json_object_new_object();
  json_object_object_add(ensure, "type", json_object_new_string("ensure"));
  json_object_object_add(ensure, "name",
                         json_object_new_string("easytier_core"));
  json_object *command = json_object_new_array();
  json_object_array_add(
      command, json_object_new_string("bin/onekvm-easytier-controller"));
  json_object_array_add(command, json_object_new_string("--run"));
  json_object_object_add(ensure, "command", command);
  json_object_object_add(ensure, "restart",
                         json_object_new_string("on-failure"));
  json_object_array_add(actions, ensure);
  json_object_object_add(plan, "actions", actions);
  std::cout << json_object_to_json_string_ext(plan, JSON_C_TO_STRING_PLAIN)
            << '\n'
            << std::flush;
  json_object_put(plan);
}

void PrintStatus(const Settings &settings) {
  json_object *response = json_object_new_object();
  json_object_object_add(response, "ready",
                         json_object_new_boolean(Ready(settings)));
  json_object_object_add(response, "serviceEnabled",
                         json_object_new_boolean(settings.service_enabled));
  json_object_object_add(
      response, "networkName",
      json_object_new_string(settings.network_name.c_str()));
  json_object_object_add(response, "dhcp",
                         json_object_new_boolean(settings.dhcp));
  json_object_object_add(response, "ipv4",
                         json_object_new_string(settings.ipv4.c_str()));
  json_object_object_add(response, "hostname",
                         json_object_new_string(settings.hostname.c_str()));
  json_object_object_add(response, "listenPort",
                         json_object_new_int(settings.listen_port));
  json_object *peers = json_object_new_array();
  for (const std::string &peer : settings.peer_uris)
    json_object_array_add(peers, json_object_new_string(peer.c_str()));
  json_object_object_add(response, "peers", peers);
  json_object *plan = json_object_new_object();
  json_object_object_add(plan, "actions", json_object_new_array());
  json_object_object_add(plan, "response", response);
  std::cout << json_object_to_json_string_ext(plan, JSON_C_TO_STRING_PLAIN)
            << '\n'
            << std::flush;
  json_object_put(plan);
}

bool Controller(std::string &error) {
  json_object *root = ReadCinJSON(error);
  if (root == nullptr || !json_object_is_type(root, json_type_object)) {
    if (root != nullptr)
      json_object_put(root);
    return false;
  }
  json_object *event_value = nullptr;
  json_object *id_value = nullptr;
  json_object *enabled_value = nullptr;
  const bool valid =
      json_object_object_get_ex(root, "event", &event_value) &&
      json_object_is_type(event_value, json_type_string) &&
      json_object_object_get_ex(root, "extension_id", &id_value) &&
      json_object_is_type(id_value, json_type_string) &&
      json_object_object_get_ex(root, "enabled", &enabled_value) &&
      json_object_is_type(enabled_value, json_type_boolean);
  if (!valid || std::string_view(json_object_get_string(id_value)) !=
                    kExtensionID) {
    json_object_put(root);
    error = "unsupported controller event";
    return false;
  }
  const std::string event = json_object_get_string(event_value);
  const bool enabled = json_object_get_boolean(enabled_value) != 0;
  if (event == "invoke") {
    json_object *method_value = nullptr;
    const bool method_valid =
        json_object_object_get_ex(root, "method", &method_value) &&
        json_object_is_type(method_value, json_type_string);
    const std::string method =
        method_valid ? json_object_get_string(method_value) : "";
    json_object_put(root);
    if (method != "status") {
      error = "unknown public method " + method;
      return false;
    }
    Settings settings;
    if (!LoadSettings(true, settings, error))
      return false;
    PrintStatus(settings);
    return true;
  }
  json_object_put(root);
  if (event != "enable" && event != "disable" && event != "reconcile" &&
      event != "config_changed") {
    error = "unsupported controller lifecycle event " + event;
    return false;
  }
  Settings settings;
  if (!LoadSettings(true, settings, error))
    return false;
  if (!enabled || event == "disable" || !Ready(settings)) {
    PrintPlan(nullptr, false);
    return true;
  }
  if (event == "config_changed")
    PrintReplacePlan();
  else
    PrintPlan(nullptr, true);
  return true;
}

int Run(std::string &error) {
  Settings settings;
  if (!LoadSettings(false, settings, error))
    return 1;
  if (!Ready(settings)) {
    error = "EasyTier client is not configured";
    return 1;
  }
  const std::string data = DataPath();
  std::error_code filesystem_error;
  std::filesystem::create_directories(data, filesystem_error);
  if (filesystem_error) {
    error = "create data directory: " + filesystem_error.message();
    return 1;
  }
  if (chmod(data.c_str(), 0750) != 0) {
    error = "set data directory permissions: " +
            std::string(std::strerror(errno));
    return 1;
  }
  const std::string config_path = data + "/easytier.toml";
  if (!WriteSecret(config_path, RenderConfig(settings), error))
    return 1;
  Exec({Executable("easytier-core"), "-c", config_path});
}

} // namespace

int main(int argc, char **argv) {
  std::string error;
  int result = 1;
  if (argc == 2 && std::string_view(argv[1]) == "--controller") {
    result = Controller(error) ? 0 : 1;
  } else if (argc == 2 && std::string_view(argv[1]) == "--health") {
    Settings settings;
    result = LoadSettings(true, settings, error) ? 0 : 1;
  } else if (argc == 2 && std::string_view(argv[1]) == "--run") {
    result = Run(error);
  } else {
    error =
        "usage: onekvm-easytier-controller [--controller|--health|--run]";
  }
  if (result != 0)
    std::cerr << "onekvm-easytier-controller: " << error << '\n';
  return result;
}

#include "infra/VultrDeployUserdata.hpp"

#include <algorithm>
#include <cctype>

namespace avacli {

namespace {

std::string toLower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Ubuntu 24.04 + avacli from source (see GROK_NOTES.md / README build steps).
// Public clone URL may change; override on the instance if the repo is private.
constexpr const char* kAgentOnlyYaml = R"YAML(#cloud-config
write_files:
  - path: /root/bootstrap-avacli.sh
    permissions: '0755'
    content: |
      #!/bin/bash
      set -euo pipefail
      export DEBIAN_FRONTEND=noninteractive
      apt-get update
      apt-get install -y build-essential cmake git libcurl4-openssl-dev libssl-dev
      mkdir -p /opt
      rm -rf /opt/avalynn-ai
      if ! git clone --depth 1 https://github.com/iBerry420/avalynn-ai.git /opt/avalynn-ai; then
        echo "git clone failed (private repo?). Install manually; deps are already present." >> /root/avacli-bootstrap.log
        exit 0
      fi
      cd /opt/avalynn-ai
      chmod +x build.sh
      ./build.sh
      install -m 0755 build/avacli /usr/local/bin/avacli
      printf '%s\n' '[Unit]' 'Description=avacli embedded server' \
        'After=network-online.target' 'Wants=network-online.target' '' \
        '[Service]' 'Type=simple' \
        'ExecStart=/usr/local/bin/avacli serve --host 0.0.0.0 --port 8080' \
        'WorkingDirectory=/root' 'Restart=on-failure' '' \
        '[Install]' 'WantedBy=multi-user.target' \
        >/etc/systemd/system/avacli.service
      systemctl daemon-reload
      systemctl enable --now avacli.service
runcmd:
  - /root/bootstrap-avacli.sh
)YAML";

constexpr const char* kLampYaml = R"YAML(#cloud-config
write_files:
  - path: /root/bootstrap-lamp-avacli.sh
    permissions: '0755'
    content: |
      #!/bin/bash
      set -euo pipefail
      export DEBIAN_FRONTEND=noninteractive
      apt-get update
      apt-get install -y apache2 mariadb-server php libapache2-mod-php php-mysql php-cli php-curl php-mbstring php-xml redis-server
      apt-get install -y build-essential cmake git libcurl4-openssl-dev libssl-dev
      systemctl enable apache2 mariadb redis-server
      systemctl start apache2 mariadb redis-server
      mkdir -p /opt
      rm -rf /opt/avalynn-ai
      if git clone --depth 1 https://github.com/iBerry420/avalynn-ai.git /opt/avalynn-ai; then
        cd /opt/avalynn-ai
        chmod +x build.sh
        ./build.sh
        install -m 0755 build/avacli /usr/local/bin/avacli
        printf '%s\n' '[Unit]' 'Description=avacli embedded server' \
          'After=network-online.target apache2.service' 'Wants=network-online.target' '' \
          '[Service]' 'Type=simple' \
          'ExecStart=/usr/local/bin/avacli serve --host 0.0.0.0 --port 8080' \
          'WorkingDirectory=/root' 'Restart=on-failure' '' \
          '[Install]' 'WantedBy=multi-user.target' \
          >/etc/systemd/system/avacli.service
        systemctl daemon-reload
        systemctl enable --now avacli.service
      else
        echo "git clone failed; LAMP stack is ready. Install avacli manually." >> /root/avacli-bootstrap.log
      fi
runcmd:
  - /root/bootstrap-lamp-avacli.sh
)YAML";

} // namespace

std::string vultrDeployUserDataForProfile(const std::string& profileId) {
    std::string id = toLower(profileId);
    if (id.empty() || id == "blank")
        return "";
    if (id == "agent-only")
        return std::string(kAgentOnlyYaml);
    if (id == "lamp-stack")
        return std::string(kLampYaml);
    return "";
}

} // namespace avacli

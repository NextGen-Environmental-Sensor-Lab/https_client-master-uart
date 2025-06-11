# https_client-master-uart

## Prerequisites

Before getting started, make sure you have a proper nRF Connect SDK development environment with NCS version v2.9.1.
Follow the official
[Installation guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/install_ncs.html).

### IMPORTANT PATCH 
For OTA to work with `github.com`, modify the file `nrf/subsys/net/lib/download_client/src/download_client.c` set sockopt to verify = OPTIONAL;

Above file `download_client.c` can be found in the `nrf` folder where the NCS toolchain has been installed. 

For mac users, it is typically at `/opt/nordic/ncs/v2.9.1/`

For linux users, it is typically at `~/ncs/v2.9.1/`

For Windows users, it is typically at `C:/ncs/v2.9.1/`

## OTA instructions 

### Uploding the image to GitHub.com
1. Pristine build the project.
2. OTA image is generated as `zephyr.signed.bin` in `/build/https_client-master-uart/zephyr/` folder.
3. Copy `zephyr.signed.bin` to `/compiled_ota_images` folder and rename it to your liking -- include versioning in the name to keep track!!!

### Triggering the update
On NGENS nodeRED `LTE_rg15` flow edit `prepIncomingData` node contents, especially the lines below, to setup OTA.

```
msg.headers = {};
msg.headers['fw_ver'] = 'v0.0.2';
msg.headers['fw_ota_url'] = "https://raw.githubusercontent.com/NextGen-Environmental-Sensor-Lab/https_client-master-uart/refs/heads/lte_conn_improvement/compiled_ota_images/https_client_v0.0.2.bin";
```

The sensor reads the `fw_ver` header and verifies whether its version matches. If there is a mismatch, it performs OTA using the link in the 
`fw_ota_url` header. The `fw_ota_url` value must include `https://` in the beginning, and must be a direct link to the file that returns code 200. 



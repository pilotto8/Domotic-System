# ot_udp_sender example

This sample (`ot_udp_sender.c`) demonstrates sending UDP packets using the OpenThread C API from the `esp_ot_cli` example.

How to use:

1. Build and flash the `esp_ot_cli` example (the new file is included in `main/CMakeLists.txt`):

```powershell
idf.py -p COMx flash monitor
```

2. On another Thread node (or CLI on the same device), ensure there is a listener bound on the destination address and port. From the OT CLI, commands look like:

```
udp open
udp bind :: 1234
```

3. The `ot_udp_sender.c` task will wait for the OpenThread instance and attachment then periodically send a message to `fdde:ad00:beef::2:1234` (update `DEST_ADDR`/`DEST_PORT` in the file for your network) every 10 seconds.

Notes:

- If you prefer to test via CLI, use `udp send fdde:ad00:beef::2 1234 hello`.
- The sender uses `otUdpOpen` and `otUdpSend` (Check `include/openthread/udp.h` in the OpenThread docs for full API details).

Adjust `DEST_ADDR` and `DEST_PORT` in `main/ot_udp_sender.c` to point to a reachable destination in your Thread network.

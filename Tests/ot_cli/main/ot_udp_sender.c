/*
 * Example: minimal OpenThread UDP sender for esp_ot_cli
 *
 * This file demonstrates how to programmatically send UDP packets using
 * the OpenThread C API from an ESP-IDF application that already initializes
 * and runs the OpenThread stack (like `esp_ot_cli`).
 *
 * Steps:
 *  - Wait for the OpenThread instance to be available
 *  - Wait for the device to be attached to a Thread network (optional)
 *  - Open a UDP socket and send periodic messages to a destination IPv6
 *
 * Adjust DEST_ADDR / DEST_PORT to match your test topology. The example
 * periodically sends a short text payload to the destination.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_openthread.h"
#include "esp_openthread_types.h"

#include <openthread/instance.h>
#include <openthread/udp.h>
#include <openthread/message.h>
#include <openthread/ip6.h>
#include <openthread/thread.h>

static const char *TAG = "ot_udp_sender";

/* Default destination: update to match your test node */
#define DEST_ADDR "fdde:ad00:beef::2"
#define DEST_PORT 1234
#define SEND_INTERVAL_MS 10000

/* simple UDP receive callback to print incoming payloads */
static void udp_receive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    (void)aContext;
    char buf[256];
    int length = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
    if (length > 0) {
        buf[length] = '\0';
        ESP_LOGI(TAG, "UDP recv %d bytes from %x:%u: %s", length,
                 (int)aMessageInfo->mPeerAddr.mFields.m16[7], aMessageInfo->mPeerPort, buf);
    } else {
        ESP_LOGI(TAG, "UDP recv but no payload");
    }
}

static void udp_sender_task(void *arg)
{
    (void)arg;

    otInstance *instance = NULL;

    /* Wait until OpenThread instance is initialised by the other task */
    while ((instance = esp_openthread_get_instance()) == NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGD(TAG, "waiting for OpenThread instance...");
    }

    ESP_LOGI(TAG, "Got OpenThread instance: %p", instance);

    /* Optionally wait until attached */
    while (otThreadGetDeviceRole(instance) == OT_DEVICE_ROLE_DISABLED ||
           otThreadGetDeviceRole(instance) == OT_DEVICE_ROLE_DETACHED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGD(TAG, "waiting to attach to Thread network...");
    }
    ESP_LOGI(TAG, "Attached to a Thread network (role=%d)", otThreadGetDeviceRole(instance));

    otUdpSocket socket;
    memset(&socket, 0, sizeof(socket));

    /* Open a UDP socket and register the receive handler */
    otError err = otUdpOpen(instance, &socket, udp_receive, NULL);
    if (err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "otUdpOpen failed: %d", err);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP socket opened");

    while (1) {
        /* Build a short text payload */
        const char *payload = "Hello from esp_ot_udp_sender";
        size_t payload_len = strlen(payload);

        otMessageSettings settings = { .mLinkSecurityEnabled = true, .mPriority = OT_MESSAGE_PRIORITY_NORMAL };
        otMessage *msg = otUdpNewMessage(instance, &settings);
        if (msg == NULL) {
            ESP_LOGE(TAG, "otUdpNewMessage returned NULL (no buffers)");
            vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
            continue;
        }

        err = otMessageAppend(msg, payload, payload_len);
        if (err != OT_ERROR_NONE) {
            ESP_LOGE(TAG, "otMessageAppend failed: %d", err);
            otMessageFree(msg);
            vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
            continue;
        }

        otMessageInfo info;
        memset(&info, 0, sizeof(info));

        {
            otError parse_err = otIp6AddressFromString(DEST_ADDR, &info.mPeerAddr);
            if (parse_err != OT_ERROR_NONE) {
                ESP_LOGE(TAG, "Invalid DEST_ADDR string: %s (err=%d)", DEST_ADDR, parse_err);
                otMessageFree(msg);
                break; // nothing to do
            } else {
                char addr_str[OT_IP6_ADDRESS_STRING_SIZE];
                otIp6AddressToString(&info.mPeerAddr, addr_str, sizeof(addr_str));
                ESP_LOGI(TAG, "Parsed dst address: %s", addr_str);
            }
        }
        info.mPeerPort = DEST_PORT;

        ESP_LOGI(TAG, "Sending UDP to %s:%u", DEST_ADDR, DEST_PORT);
        err = otUdpSend(instance, &socket, msg, &info);
        if (err != OT_ERROR_NONE) {
            ESP_LOGE(TAG, "otUdpSend failed: %d", err);
            otMessageFree(msg); // on failure, we keep ownership
        } else {
            ESP_LOGI(TAG, "Message enqueued for sending");
        }

        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }

    /* cleanup (not really reached) */
    otUdpClose(instance, &socket);
    vTaskDelete(NULL);
}

void ot_udp_sender_init(void)
{
    /* Create a task that will wait for OpenThread instance and then send UDP packets.
     * This must be called once at or after OpenThread initialization.
     */
    xTaskCreate(udp_sender_task, "ot_udp_sender", 4096, NULL, 5, NULL);
}

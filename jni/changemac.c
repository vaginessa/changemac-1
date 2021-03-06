#include <android/log.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_TAG "changemac"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

/* https://android.googlesource.com/kernel/msm/+/android-8.0.0_r0.4/Documentation/devicetree/bindings/wcnss/wcnss-wlan.txt#65 */
#ifndef WCNSS_DEVICE_ADDRESS
#define WCNSS_DEVICE_ADDRESS "fb000000"
#endif
#ifndef WCNSS_DEVICE_COMPATIBLE
#define WCNSS_DEVICE_COMPATIBLE "qcom,wcnss-wlan"
#endif

#ifndef WCNSS_SYSFS_PATH
#define WCNSS_SYSFS_PATH "/sys/bus/platform/devices/" WCNSS_DEVICE_ADDRESS "." WCNSS_DEVICE_COMPATIBLE
#endif

#ifndef WCNSS_MAC_ADDRESS_FILE
#define WCNSS_MAC_ADDRESS_FILE WCNSS_SYSFS_PATH "/wcnss_mac_addr"
#endif

#define MAC_OUI_LENGTH    3
#define MAC_CUSTOM_LENGTH 3

static const uint8_t VENDORS[][MAC_OUI_LENGTH] = {
    { 0x00, 0x26, 0x37 }, /* SAMSUNG ELECTRO MECHANICS CO., LTD. */
    { 0x08, 0x00, 0x28 }, /* Texas Instruments */
    { 0x44, 0x74, 0x6c }, /* Sony Mobile Communications AB */
    { 0x64, 0x16, 0xf0 }, /* HUAWEI TECHNOLOGIES CO.,LTD */
    { 0x80, 0x19, 0x34 }, /* Intel Corporate */
    { 0x84, 0x10, 0x0d }, /* Motorola Mobility LLC, a Lenovo Company */
    { 0x84, 0x3a, 0x4b }, /* Intel Corporate */
    { 0xb0, 0xc0, 0x90 }, /* Chicony Electronics Co., Ltd. */
    { 0xe4, 0xb3, 0x18 }, /* Intel Corporate */
    { 0xf0, 0x25, 0xb7 }, /* SAMSUNG ELECTRO-MECHANICS(THAILAND) */
    { 0xf8, 0xa9, 0xd0 }, /* LG Electronics (Mobile Communications) */
};

#define VENDORS_COUNT (sizeof(VENDORS) / sizeof(*VENDORS))

static bool random_buffer(FILE *stream, void *buffer, size_t size);
static bool random_uniform(FILE *stream, uintmax_t n, uintmax_t *out);
static const uint8_t *random_vendor(FILE *stream);

int main(int argc, char **argv) {
    FILE *urandom = fopen("/dev/urandom", "r");
    if (urandom == NULL) {
	/* This should never happen, probably due to SELinux */
	ALOGE("Could not open /dev/urandom: %d", errno);
	return 1;
    }

    const uint8_t *vendor = random_vendor(urandom);

    uint8_t custom[MAC_CUSTOM_LENGTH];
    random_buffer(urandom, custom, sizeof(custom));

    char mac_address[3 * (MAC_OUI_LENGTH + MAC_CUSTOM_LENGTH)];
    snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
	    vendor[0], vendor[1], vendor[2],
	    custom[0], custom[1], custom[2]);

    ALOGI("Generated MAC address: %s", mac_address);

    FILE *output = fopen(WCNSS_MAC_ADDRESS_FILE, "w");
    if (output == NULL) {
	/* There's nothing we can do about it, the MAC address will probably be leaked */
	ALOGE("Could not open output file: %d", errno);
	return 1;
    }

    if (fputs(mac_address, output) < 0) {
	ALOGE("Could not write to output file: %d", errno);

	if (fclose(output) != 0) {
	    ALOGE("Could not close output file: %d", errno);
	}

	return 1;
    }

    if (fclose(output) != 0) {
	ALOGE("Could not close output file: %d", errno);
	return 1;
    }

    return 0;
}

bool random_buffer(FILE *stream, void *buffer, size_t size) {
    return (fread(buffer, 1, size, stream) == size);
}

bool random_uniform(FILE *stream, uintmax_t n, uintmax_t *out) {
    uintmax_t x, max = UINTMAX_MAX - (UINTMAX_MAX % n);

    do {
	if (!random_buffer(stream, &x, sizeof(x))) {
	    return false;
	}
    } while (x >= max);

    *out = (x / (max / n));
    return true;
}

const uint8_t *random_vendor(FILE *stream) {
    uintmax_t x = 0;

    if (!random_uniform(stream, VENDORS_COUNT, &x)) {
	/* Ignore the error to avoid leaking the MAC address */
	ALOGE("Could not read random bytes from stream: %d", errno);;
    }

    ALOGI("Selected vendor index: %ju\n", x);
    return VENDORS[x];
}

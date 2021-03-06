/**
 *
 * \file
 *
 * \brief WINC1500 Example.
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

/** \mainpage
 * \section intro Introduction
 * This example demonstrates how to connect to the Parse server
 * using the WINC1500 Wi-Fi module.<br>
 * It uses the following hardware:
 * - the SAMD21 Xplained Pro.
 * - the WINC1500 on EXT1.
 *
 * \section files Main Files
 * - main.c : Initialize the WINC1500 Wi-Fi module and connect to the Parse server.
 *
 * \section usage Usage
 * -# Configure below code in this file for AP and the Parse information to be connected.
 * \code
 *    #define NETWORK_SSID "YOUR_NETWORK_IS_HERE"
 *    #define NETWORK_PASS "YOUR_NETWORK_PASSWORD_IS_HERE"
 *    #define NETWORK_SECURITY_TYPE 2
 *    #define APPLICATION_ID           "YOUR_PARSE_APPLICATION_ID_HERE"
 *    #define CLIENT_KEY               "YOUR_PARSE_CLIENT_KEY_IS_HERE"
 *    #define INSTALLATION_ID          "INSTALLATION_ID_OF_YOUR_DEVICE_HERE"
 *    #define RESTRICTED_SESSION_TOKEN "YOUR_RESTRICTED_SESSION_TOKEN"
 * \endcode
 * -# Build the program and download it into the board.
 * -# On the computer, open and configure a terminal application as following.
 * \code
 *    Baud Rate : 115200
 *    Data : 8bit
 *    Parity bit : none
 *    Stop bit : 1bit
 *    Flow control : none
 * \endcode
 *
 * \section compinfo Compilation Information
 * This software was written for the GNU GCC compiler using Atmel Studio 6.2
 * Other compilers may or may not work.
 *
 * \section contactinfo Contact Information
 * For further information, visit
 * <A href="http://www.atmel.com">Atmel</A>.\n
 */

#include "asf.h"
#include "parse.h"
#include "parse/parse_impl.h"
#include "parse/nm_fluffy.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "conf_nvm.h"
#include "simplejson.h"

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- Atmel WINC1500 : Fluffy --"STRING_EOL	\
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/* Parse instance */
ParseClient parseClient = NULL;

/** UART module for debug. */
static struct usart_module cdc_uart_module;

/** Instance of Timer module. */
struct tcc_module tccModuleForBlink;

static uint8_t gButtonPinStat = 1;
int gButtonClick = false;

#define MAX_MSG_SIZE 128
#define MAX_TYPE_SIZE 20

#define LED_OFF 0
#define LED_ON 1

const char *ledStates[] = {"off", "on", "blink"};

#define STATE_OFF 0
#define STATE_ON 1
#define STATE_BLINK 2

static char userObjectId[OBJECT_ID_MAX_LEN + 1] = { 0, };
static char installationObjectId[OBJECT_ID_MAX_LEN + 1];
static char modelObjectId[OBJECT_ID_MAX_LEN + 1];

static int device_state = STATE_BLINK;

#define NETWORK_SSID "YOUR_NETWORK_IS_HERE"
#define NETWORK_PASS "YOUR_NETWORK_PASSWORD_IS_HERE"
/* 0 - no security, 1 - WEP, 2 - WPA */
#define NETWORK_SECURITY_TYPE 2

#define APPLICATION_ID           "YOUR_PARSE_APPLICATION_ID_HERE"
#define CLIENT_KEY               "YOUR_PARSE_CLIENT_KEY_IS_HERE"
#define INSTALLATION_ID          "INSTALLATION_ID_OF_YOUR_DEVICE_HERE"
#define RESTRICTED_SESSION_TOKEN "YOUR_RESTRICTED_SESSION_TOKEN"

static void saveCurrentDeviceState(ParseClient client, int device_state);

static void requestCallback(ParseClient client, int error, int httpStatus, const char *httpResponseBody)
{
	if (0 != error) {
		printf("requestCallback / error code %d / Error !!!!!\r\n", error);
		return;
	}

	printf("requestCallback : 0x%x, %d, %d, %s\r\n\r\n", client, error, httpStatus, httpResponseBody);
}

void pushCallback(ParseClient client, int error, const char *data)
{
	if (0 != error) {
		printf("pushCallback / error code %d / Error !!!!!\r\n", error);
		return;
	}

	printf("pushCallback : 0x%x, %d, %s\r\n\r\n", client, error, data);

	char msg[MAX_MSG_SIZE];
	char msgType[MAX_TYPE_SIZE];

	if (simpleJsonProcessor(data, "data", msg, MAX_MSG_SIZE) != 0) {
		if (simpleJsonProcessor(msg, "alert", msgType, MAX_TYPE_SIZE) != 0) {
			if (strcmp("blink", msgType) == 0) {
				device_state = STATE_BLINK;
			} else if (strcmp("off", msgType) == 0) {
				device_state = STATE_OFF;
			} else if (strcmp("on", msgType) == 0) {
				device_state = STATE_ON;
			} else {
				printf("INFO: unknown state '%s'\n", msgType);
			}

			saveCurrentDeviceState(parseClient, device_state);
		}
	}
}

static void saveCurrentDeviceState(ParseClient client, int device_state)
{
	ParseClientInternal *clientInternal = (ParseClientInternal *)client;

	if (installationObjectId[0] != 0 && userObjectId[0] != 0) {
		printf("saveCurrentDeviceState is called\r\n");

		char objectJson[1024];
		sprintf(objectJson, "{\"installationId\": \"%s\", \"value\": {\"state\": \"%s\"}, \"alarm\": true, \"ACL\":{ \"%s\": { \"read\": true, \"write\": true}}}",
				installationObjectId, ledStates[device_state], userObjectId);
		parseSendRequest(client, "POST", "/1/classes/Event", objectJson, NULL);
	} else {
		if (clientInternal->installationObjectId[0] == 0) {
			printf("saveCurrentDeviceState is called / clientInternal->installationObjectId is NULL Error !!!!!\r\n");
		}

		if (userObjectId[0] == 0) {
			printf("saveCurrentDeviceState is called / userObjectId is NULL Error !!!!!\r\n");
		}
	}
}

static void button_pressed()
{
	printf("button_pressed =========\r\n");
	gButtonClick = true;
}

/**
 * \brief Timer callback function.
 */
static void timerCallback(struct tcc_module *const module)
{
	if (device_state == STATE_BLINK) {
		if (port_pin_get_output_level(LED_0_PIN)) {
			port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		} else {
			port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
		}
	}
}

/** Callback function for the EXTINT driver, called when an external interrupt
 *  detection occurs.
 */
static void extint_cb(void)
{
	bool pin_state = port_pin_get_input_level(BUTTON_0_PIN);

	if (!pin_state) {
		gButtonPinStat = pin_state;
	} else {
		if (!gButtonPinStat) {
			button_pressed();
		}
	}
}

/**
 * \brief Configures and registers the External Interrupt callback function
 * with the driver.
 */
static void configure_eic_callback(void)
{
	extint_register_callback(extint_cb,
			BUTTON_0_EIC_LINE,
			EXTINT_CALLBACK_TYPE_DETECT);

	extint_chan_enable_callback(BUTTON_0_EIC_LINE,
			EXTINT_CALLBACK_TYPE_DETECT);
}

/**
 * *\brief Configures the External Interrupt Controller
 * to detect changes in the board button state.
 */
static void configure_extint(void)
{
	struct extint_chan_conf eint_chan_conf;
	extint_chan_get_config_defaults(&eint_chan_conf);

	eint_chan_conf.gpio_pin           = BUTTON_0_EIC_PIN;
	eint_chan_conf.gpio_pin_mux       = BUTTON_0_EIC_MUX;
	eint_chan_conf.detection_criteria = EXTINT_DETECT_BOTH;
	eint_chan_conf.filter_input_signal = true;
	extint_chan_set_config(BUTTON_0_EIC_LINE, &eint_chan_conf);
}

/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	usart_conf.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	usart_conf.baudrate    = 115200;

	stdio_serial_init(&cdc_uart_module, EDBG_CDC_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}

/**
 * \brief Configure LED0, turn it off
 */
static void config_led(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LED_0_PIN, &pin_conf);
	port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
}

/**
 * \brief Configure Timer module.
 */
static void configure_blink_timer(void)
{
	/**
	 * Timer period is 1s = Prescaler(256) * Period(46875) / Clock(12Mhz).
	 */
	struct tcc_config tcc_conf;
	tcc_get_config_defaults(&tcc_conf, TCC1);
	tcc_conf.counter.period = 46875;
	tcc_conf.counter.clock_prescaler = TCC_CLOCK_PRESCALER_DIV256;

	tcc_init(&tccModuleForBlink, TCC1, &tcc_conf);
	/* Register timer callback for the Request RSSI. */
	tcc_register_callback(&tccModuleForBlink, timerCallback, TCC_CALLBACK_CHANNEL_1);
	tcc_enable(&tccModuleForBlink);
	tcc_enable_callback(&tccModuleForBlink, TCC_CALLBACK_CHANNEL_1);
}

static void blinkGetUserObjectIdCallback(ParseClient client, int error, int httpStatus, const char *httpResponseBody)
{
	printf("blinkGetUserObjectIdCallback : 0x%x, %d, %d, %s\r\n\r\n", client, error, httpStatus, httpResponseBody);

	userObjectId[0] = '\0';
	if ((error == 0) && (httpStatus >= 200 && httpStatus < 300)) {
		simpleJsonProcessor(httpResponseBody, "objectId", userObjectId, OBJECT_ID_MAX_LEN);
	}
}

static void blinkGetInstallationObjectIdByIdCallback(ParseClient client, int error, int httpStatus, const char *httpResponseBody)
{
	installationObjectId[0] = '\0';
	if ((error == 0) && (httpStatus >= 200 && httpStatus < 300)) {
		char queryResults[1024] = {0};
		short status = simpleJsonProcessor(httpResponseBody, "results", queryResults, sizeof(queryResults));
		if ((status != 0) && (queryResults[1] == '{')) {
			simpleJsonProcessor(queryResults + 1, "objectId", installationObjectId, sizeof(installationObjectId));
		}
	}
}

static void blinkGetInstallationObjectId(ParseClient client)
{
	if (installationObjectId[0] == '\0') {
		char query[100];
		sprintf(query, "/1/installations?where={\"installationId\":\"%s\"}", parseGetInstallationId(client));
		parseSendRequest(client, "GET", query, NULL, blinkGetInstallationObjectIdByIdCallback);
	}
}

static void blinkGetModelObjectIdByNameCallback(ParseClient client, int error, int httpStatus, const char *httpResponseBody)
{
	modelObjectId[0] = '\0';
	if ((error == 0) && (httpStatus >= 200 && httpStatus < 300)) {
		char queryResults[1024] = {0};
		short status = simpleJsonProcessor(httpResponseBody, "results", queryResults, sizeof(queryResults));
		if ((status != 0) && (queryResults[1] == '{')) {
			simpleJsonProcessor(queryResults + 1, "objectId", modelObjectId, sizeof(modelObjectId));
		}
	}
}

static void blinkGetUserObjectId(ParseClient client)
{
	if (userObjectId[0] == '\0') {
		parseSendRequest(client, "GET", "/1/users/me", NULL, blinkGetUserObjectIdCallback);
	}
}

static void blinkGetModelObjectId(ParseClient client)
{
	if (modelObjectId[0] == '\0') {
		parseSendRequest(client, "GET", "/1/classes/Model?where={\"appName\":\"fbdr000001c\"}", NULL, blinkGetModelObjectIdByNameCallback);
	}
}

static void blinkUpdateInstallation(ParseClient client)
{
	printf("Bravo!");
	blinkGetInstallationObjectId(client);

	if (strlen(installationObjectId) > 0) {
		blinkGetModelObjectId(client);
		blinkGetUserObjectId(client);

		char path[40] = {0};
		char objectJson[1024] = {0};
		sprintf(path, "/1/installations/%s", installationObjectId);
		sprintf(objectJson, "{\"deviceName\": \"%s\", \"deviceSubtype\": \"fluffy\", \"model\": {\"__type\":\"Pointer\",\"className\":\"Model\",\"objectId\":\"%s\"}, \"owner\": {\"__type\":\"Pointer\",\"className\":\"_User\",\"objectId\":\"%s\"}}",
				"Atmel SAMD21 blink", modelObjectId, userObjectId);
		parseSendRequest(client, "PUT", path, objectJson, NULL);
	}
}

/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
	/* Initialize the board. */
	system_init();

	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);

	/* Set external interrupt of button state */
	configure_extint();
	configure_eic_callback();
	system_interrupt_enable_global();

	/* Initialize the LED0. */
	config_led();

	/* Initialize the Timer. */
	configure_blink_timer();

	/* Configure Non-Volatile Memory */
	configure_nvm();

	if (LOCAL_PARSE_SUCCESS != initWifiModule()) {
		printf("main / initWifiModule Error !!!!!\r\n");
		return -1;
	}

	loadSettingsOrProvisionBoard(NETWORK_SSID, NETWORK_PASS, NETWORK_SECURITY_TYPE);
	while (1) {
		if (M2M_WIFI_MODE_STA == localParseEventLoop(NULL)) {
			printf("main / WiFi connection is done\r\n");
			break;
		}
	}

	parseClient = parseInitialize(APPLICATION_ID, CLIENT_KEY);
	/* Uncomment the following line if you want to control blink on pre-created device. */
	/* parseSetInstallationId(parseClient, INSTALLATION_ID); */
	parseSetSessionToken(parseClient, RESTRICTED_SESSION_TOKEN);

	blinkUpdateInstallation(parseClient);

	if (parseClient) {
		printf("main / parseClient 0x%x\r\n", parseClient );

		parseSetPushCallback(parseClient, pushCallback);
		parseStartPushService(parseClient);

		while (1) {
			parseRunPushLoop(parseClient);

			ParseClientInternal *clientInternal = (ParseClientInternal *)parseClient;
			if (installationObjectId[0] != 0 && userObjectId[0] == 0) {
				blinkGetUserObjectId(parseClient);
			}

			if (gButtonClick == true) {
				gButtonClick = false;
				switch (device_state) {
				case STATE_BLINK:
					device_state = STATE_OFF; break;

				case STATE_OFF:
					device_state = STATE_ON; break;

				case STATE_ON:
					device_state = STATE_BLINK; break;
				}

				saveCurrentDeviceState(parseClient, device_state);
			}

			if (device_state == STATE_BLINK) {
				/* please check the timerCallback */
			} else if (device_state == STATE_OFF) {
				if (!port_pin_get_output_level(LED_0_PIN)) {
					port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
				}
			} else if (device_state == STATE_ON) {
				if (port_pin_get_output_level(LED_0_PIN)) {
					port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
				}
			} else {
			}
		}
	}

	return 0;
}

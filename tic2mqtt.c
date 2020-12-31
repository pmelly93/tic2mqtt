#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>

#include <mosquitto.h>
/* See https://mosquitto.org/api/files/mosquitto-h.html for mosquitto library API reference. */

#include "broker_helper.h"
#include "homie_helper.h"
#include "tic2mqtt.h"

#define TIC2MQTT_VERSION "1.0.0"

#define STX 0x02
#define ETX 0x03

#define DEFAULT_TTY "/dev/ttyS0"
#define TIC_BAUDRATE B1200
#define TIC_FRAME_MAX 512

#define DEFAULT_HOST      "localhost"
#define DEFAULT_PORT      1883
#define DEFAULT_KEEPALIVE 60

/* Tags for 'compteur monophasé multitarif'. */

static struct tag_desc tag_descs[] =
{
    { "ADCO",    12 },
    { "OPTARIF",  4 },
    { "ISOUSC",   2 },

    { "BASE",     9 },

    { "HCHC",     9 },
    { "HCHP",     9 },

    { "EJPHN",    9 },
    { "EJPHPM",   9 },

    { "BBRHCJB",  9 },
    { "BBRHPJB",  9 },
    { "BBRHCJW",  9 },
    { "BBRHPJW",  9 },
    { "BBRHCJR",  9 },
    { "BBRHPJR",  9 },

    { "PEJP",     2 },
    { "PTEC",     4 },
    { "DEMAIN",   4 },
    { "IINST",    3 },
    { "ADPS",     3 },
    { "IMAX",     3 },
    { "PAPP",     5 },
    { "HHPHC",    1 },
    { "MOTDETAT", 6 },

    { NULL, 0 } /* End of table marker. */
};

/* Values for enum attributes of Homie property 'tic'. */

static const char * const values_optarif[] = {
    "BASE", /* Option Base. */
    "HC..", /* Option Heures Creuses. */
    "EJP.", /* Option EJP. */
    "BBRx", /* Option Tempo. */
    NULL
};

static const char * const values_ptec[] = {
    "TH..", /* Toutes les Heures. */
    "HC..", /* Heures Creuses. */
    "HP..", /* Heures Pleines. */
    "HN..", /* Heures Normales. */
    "PM..", /* Heures de Pointe Mobile. */
    "HCJB", /* Heures Creuses Jours Bleus. */
    "HCJW", /* Heures Creuses Jours Blancs (White). */
    "HCJR", /* Heures Creuses Jours Rouges. */
    "HPJB", /* Heures Pleines Jours Bleus. */
    "HPJW", /* Heures Pleines Jours Blancs (White). */
    "HPJR", /* Heures Pleines Jours Rouges. */
    NULL
};

static const char * const values_demain[] = {
    "----", /* Couleur du lendemain non connue. */
    "BLEU", /* Le lendemain est jour BLEU. */
    "BLAN", /* Le lendemain est jour BLANC. */
    "ROUG", /* Le lendemain est jour ROUGE. */
    NULL
};

/* Property attributes of Homie node 'tic'. */

static struct homie_prop_attrs tic_attrs[] = {
    { "adco",     "Adresse du compteur", HOMIE_STRING, "", NULL },
    { "optarif",  "Option tarifaire choisie", HOMIE_ENUM, "", values_optarif },
    { "isousc",   "Intensité souscrite", HOMIE_INTEGER, "A", NULL },

    { "base",     "Index option base", HOMIE_INTEGER, "Wh", NULL },

    { "hchc",     "Index option Heures Creuses: Heures Creuses", HOMIE_INTEGER, "Wh", NULL },
    { "hchp",     "Index option Heures Creuses: Heures Pleines", HOMIE_INTEGER, "Wh", NULL },

    { "ejphn",    "Index option EJP: Heures Normales", HOMIE_INTEGER, "Wh", NULL },
    { "ejphpm",   "Index option EJP: Heures de Pointe Mobile", HOMIE_INTEGER, "Wh", NULL },

    { "bbrhcjb",  "Index option Tempo: Heures Creuses Jours Bleus", HOMIE_INTEGER, "Wh", NULL },
    { "bbrhpjb",  "Index option Tempo: Heures Pleines Jours Bleus", HOMIE_INTEGER, "Wh", NULL },
    { "bbrhcjw",  "Index option Tempo: Heures Creuses Jours Blancs", HOMIE_INTEGER, "Wh", NULL },
    { "bbrhpjw",  "Index option Tempo: Heures Pleines Jours Blancs", HOMIE_INTEGER, "Wh", NULL },
    { "bbrhcjr",  "Index option Tempo: Heures Pleines Jours Rouges", HOMIE_INTEGER, "Wh", NULL },
    { "bbrhpjr",  "Index option Tempo: Heures Creuses Jours Rouges", HOMIE_INTEGER, "Wh", NULL },

    { "pejp",     "Préavis Début EJP (30 min)", HOMIE_INTEGER, "min", NULL },
    { "ptec",     "Période tarifaire en cours", HOMIE_ENUM, "", values_ptec },
    { "demain",   "Couleur du lendemain", HOMIE_ENUM, "", values_demain },
    { "iinst",    "Intensité instantanée", HOMIE_INTEGER, "A" },
    { "adps",     "Avertissement de Dépassement de Puissance Souscrite", HOMIE_INTEGER, "A", NULL },
    { "imax",     "Intensité maximale", HOMIE_INTEGER, "A", NULL },
    { "papp",     "Puissance apparente", HOMIE_INTEGER, "VA", NULL },
    { "hhphc",    "Horaire heures pleines / heures creuses", HOMIE_STRING, "", NULL },
    { "motdetat", "Mot d’état du compteur", HOMIE_STRING, "", NULL },

    { NULL, NULL, 0, NULL } /* End of table marker. */
};

static int fd_tic = -1;
static struct mosquitto *mosq_tic = NULL;
static int verbose = 0;

/**
 * @brief Open TIC TTY.
 * @param tty TTY name (/dev/ttyxx).
 * @return File descriptor to tty.
 */

static int tic_open(const char *tty)
{
    int fd;
    struct termios termios;

    if ((fd = open(tty, O_RDWR | O_NOCTTY)) < 0) {
        syslog(LOG_ERR, "Cannot open %s: %s", tty, strerror(errno));
        return -1;
    }

    tcgetattr(fd, &termios);

    /* Configure input and output speed. */
    cfsetispeed(&termios, TIC_BAUDRATE);
    cfsetospeed(&termios, TIC_BAUDRATE);

    /* Set input modes:
     * - Disable XON/XOFF flow control on input.
     * - Do not translate carriage return to newline on input.
     * - Enable input parity checking.
     * - Strip off eighth bit.
     */
      
    termios.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    termios.c_iflag |= INPCK | ISTRIP;

    /* Set output modes:
     * - Disable implementation-defined output processing (raw mode).
     */

    termios.c_oflag &= ~OPOST;

    /* Set control modes:
     * - Enable receiver.
     * - Ignore modem control lines.
     * - 7 bit.
     * - 1 stop bit.
     * - Enable parity generation on output and parity checking for input.
     * - Disable RTS/CTS (hardware) flow control.
     */

    termios.c_cflag |= CLOCAL | CREAD;
    termios.c_cflag &= ~(CSIZE | PARODD | CSTOPB);
    termios.c_cflag |=   CS7   | PARENB;
    termios.c_cflag &= ~CRTSCTS;

    /* Set local modes:
     * - Do bot generate signal when the characters INTR, QUIT, SUSP or DSUSP are received.
     * - Disable canonical mode.
     * - Do not echo input characters.
     */

    termios.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE);

    /* Set special characters:
     * - Timeout set to 8 s.
     * - Minimum number of characters for noncanonical read set to 1.
     */

    termios.c_cc[VTIME] = 80;
    termios.c_cc[VMIN]  = 0;

    tcsetattr(fd, TCSANOW, &termios);

    tcflush(fd, TCIFLUSH);
    return fd;
}

/**
 * @brief Read TIC frame on tty.
 * @param device File descriptor to tty.
 * @param frame TIC frame to be read.
 * @return 0 on success, -1 on failure.
 */

static int tic_read_frame(int device, char *frame)
{
    char *cp;
    char c;
    int res;

    tcflush(device, TCIFLUSH);

    cp = frame;

    // Wait for STX.
    do {
        res = read(device, &c, 1);
        if (res == 0) {
            syslog(LOG_ERR, "Start of TIC frame not received");
            return -1;
        }
    } while (c != STX);

    // Read until ETX.
    do {
        res = read(device, &c, 1);
        if (res == 0) {
            syslog(LOG_ERR, "End of TIC frame not received");
            return -1;
        }
        *cp++ = c;
    } while (c != ETX);

    return 0;
}

/**
 * @brief Verify the checksum of a group.
 * @param tag Tag.
 * @param data Data.
 * @param checksum Checksum.
 * @return 1 if checksum is valid, 0 else.
 */

static int tic_is_checksum_ok(const char *tag, const char *data, char checksum)
{
    unsigned char sum = ' ';
    int i;

    for (i = 0; i < strlen(tag); i++)
        sum += tag[i];

    for (i = 0; i < strlen(data); i++)
       sum += data[i];

    sum = ' ' + (sum & 0x3f);

#ifdef DEBUG
    syslog(LOG_INFO, "Checksum read: %02x computed: %02x", checksum, sum);
#endif // DEBUG

    return sum == checksum;
}

/**
 * @brief Process group if tag is found in tag_descs[]. 
 * @param tag Tag.
 * @param data Data.
 * @return 0 if successful, -1 if failure.
 */

static void tic_process_group(const char *tag, const char *data)
{
    struct tag_desc *ptag_desc;

    for (ptag_desc = tag_descs; ptag_desc->tag != NULL; ptag_desc++) {
        if (strcmp(ptag_desc->tag, tag) == 0) {
            int publish_requested = 0;
            char topic[TOPIC_MAXLEN + 1];
            int res;

            if (ptag_desc->data == NULL) {
                ptag_desc->data = calloc(1, ptag_desc->len + 1);
                if (ptag_desc->data == NULL) {
                    syslog(LOG_ERR, "Cannot alloc data for tag %s: %s\n", tag, strerror(errno));
                }
                publish_requested = 1;
            } else if (strcmp(ptag_desc->data, data) != 0) {
                publish_requested = 1;
            }

            if (publish_requested) {
                const struct homie_prop_attrs *pattrs = tic_attrs + (ptag_desc - tag_descs);

                ptag_desc->data[0] = '\0';
                strncat(ptag_desc->data, data, ptag_desc->len);

                if (verbose)
                    printf("%s=%s %s\n", tag, data, pattrs->unit);

                sprintf(topic, "%s%s/%s/%s", HOMIE_BASE_TOPIC, HOMIE_DEVICE_ID, HOMIE_NODE_ID, pattrs->prop_id);
                res = broker_publish(mosq_tic, topic, NULL, data, TIC_QOS);
                if (res != 0)
                    syslog(LOG_ERR, "Cannot publish topic %s: %s\n", topic, mosquitto_strerror(res));

            }
            return; // No more processing.
        }
    }
}

/**
 * @brief Process TIC frame (legacy mode).
 * @param frame TIC frame.
 */

static void tic_process_frame(char *frame)
{
    char *start;
    char *end;
    char *tag;
    char *data;
    char checksum;

    for (start = frame; *start != ETX; start++) {
        start++; // Skip '\n'.

        // Parse tag.
        for (end = start; *end != ' '; end++)
            ;
        *end = '\0'; // Replace ' ' with '\0'.
        tag = start;

        // Parse data.
        start = end + 1;
        for (end = start; *end != ' '; end++)
            ;
        *end = '\0'; // Replace ' ' with '\0'.
        data = start;

        // Get checksum.
        start = end + 1;
        checksum = *start;

        start++; // Skip '\r'.

        if (!tic_is_checksum_ok(tag, data, checksum))
            continue;

        tic_process_group(tag, data);

#ifdef DEBUG
        printf("%s %s %c %s\n", tag, data, checksum, tic_is_checksum_ok(tag, data, checksum) ? "OK" : "FAIL");
#endif // DEBUG
    }
}

/**
 * @brief Signal handler.
 * @param signum Signal number.
 */

static void sighandler(int signum)
{
    syslog(LOG_NOTICE, "Catch signal #%d (%s)\n", signum, strsignal(signum));
    exit(EXIT_SUCCESS);
}

/**
 * @brief Clean-up before exit.
 */

static void cleanup(void)
{
    if (fd_tic >= 0)
        close(fd_tic);

    homie_close(mosq_tic);

    broker_close(mosq_tic);

    closelog();
}

/**
 * @brief Print usage.
 * @param progname Program name.
 */

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-Hv] [-t tty] [-h host] [-p port] [-k keepalive]\n", progname);
}

/**
 * @brief Set program name from argv[0], stripping leading path, if any.
 * @param argv0 Pointer to argv[0].
 */

static void set_progname(char *argv0)
{
    char *p;

    if ((p = strrchr(argv0, '/')) != NULL)
        strcpy(argv0, p + 1); // argv[0] contains a slash.
}

/**
 * @brief Program entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return If successful, EXIT_SUCCESS is returned.
 * @return If not, EXIT_FAILURE is returned.
 */

int main(int argc, char *argv[])
{
    int opt;
    const char *tty = DEFAULT_TTY;
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    int keepalive = DEFAULT_KEEPALIVE;
    char frame[TIC_FRAME_MAX];

    set_progname(argv[0]);

    /* Decode options. */
    opterr = 1;
    while ((opt = getopt(argc, argv, "vt:h:p:k:H")) != -1) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;

        case 't':
            tty = optarg;
            break;

        case 'h':
            host = optarg;
            break;

        case 'p':
            port = atoi(optarg);
            break;

        case 'k':
            keepalive = atoi(optarg);
            break;

        case 'H':
            printf("version " TIC2MQTT_VERSION "\n");
            usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    atexit(cleanup);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);

    openlog("tic2mqtt", LOG_PID, LOG_USER);

    mosq_tic = broker_open(host, port, keepalive);
    if (mosq_tic == NULL)
        return EXIT_FAILURE;

    fd_tic = tic_open(tty);
    if (fd_tic < 0)
        return EXIT_FAILURE;

    homie_init(mosq_tic, tic_attrs);

    for (;;) {
        if (tic_read_frame(fd_tic, frame) < 0)
            return EXIT_FAILURE;
        tic_process_frame(frame);
    }

    return EXIT_SUCCESS;
}

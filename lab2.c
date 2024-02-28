/*
 * lab2.c
 * CSEE 4840 Lab 2 for Spring 2024
 *
 * Names/UNIs: Patrick Cronin (pjc2192), Kiryl Beliauski (kb3338), Daniel
 * Ivanovich (dmi2115)
 */
#include "fbputchar.h"
#include "usbkeyboard.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/fb.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

#define DIVIDING_LINE '-'

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 *
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread, drawing_thread;
void *network_thread_f(void *);
void *drawing_thread_f(void *);

volatile int drawing_thread_terminate = 0;
pthread_mutex_t write_zone_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t read_zone_mutex = PTHREAD_MUTEX_INITIALIZER;
int cursor_position;
char *write_zone_data, *read_zone_data;
screen_info screen;

// Debug mode (adds some extra logs)
#define DEBUG 1
// How many rows of text we can fit on screen using fbputs
int TEXT_ROWS_ON_SCREEN;
// How many cols of text we can fit on screen using fpbuts
int TEXT_COLS_ON_SCREEN;
// The size of read_zone_data. To be calculated after the above are populated
uint read_zone_size;

char decode_keypress(uint8_t *keycode) {
  // TODO: Implement
  return 'A';
}

int main() {
  int err;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Open the keyboard */
  if ((keyboard = openkeyboard(&endpoint_address)) == NULL) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if (inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  // Clear the screen
  fbclear();
  // Get screen info
  screen = get_fb_screen_info();

  // Calculate TEXT_COLS_ON_SCREEN and TEXT_ROWS_ON_SCREEN
  TEXT_ROWS_ON_SCREEN = screen.fb_vinfo->yres / (screen.font_height * 2);
  TEXT_COLS_ON_SCREEN = screen.fb_vinfo->xres / (screen.font_width * 2);

  // Log screen info in debug mode
  if (DEBUG) {
    printf("----SCREEN INFO----\n");
    printf("Font Width: %d\n", screen.font_width);
    printf("Font Height: %d\n", screen.font_height);
    printf("X resolution: %d\n", screen.fb_vinfo->xres);
    printf("Y resolution: %d\n", screen.fb_vinfo->yres);
    printf("X offset: %d\n", screen.fb_vinfo->xoffset);
    printf("Y offset: %d\n", screen.fb_vinfo->yoffset);
    printf("Line length: %d\n", screen.fb_finfo->line_length);
    printf("Text Rows on Screen: %d\n", TEXT_ROWS_ON_SCREEN);
    printf("Text Columns on Screen: %d\n", TEXT_COLS_ON_SCREEN);
    printf("\n");
  }

  /* Allocate memory for the read and write zones */

  // We will allow the character to enter up to BUFFER_SIZE characters
  // (including the \0) of write data in the writing panel on the bottom, as
  // that's how much data we'll allow the user to send on the socket
  if ((write_zone_data = malloc(BUFFER_SIZE * sizeof(char))) == NULL) {
    fprintf(stderr, "Error: malloc() failed for write_zone_data.\n");
    exit(1);
  }

  // The read zone can fit as many chars as there are rows x columns in the read
  // zone, plus a null terminator per row
  read_zone_size =
      (TEXT_ROWS_ON_SCREEN + 1) * (TEXT_COLS_ON_SCREEN - 3) * sizeof(char);
  if ((read_zone_data = malloc(read_zone_size)) == NULL) {
    fprintf(stderr, "Error: malloc() failed for read_zone_data.\n");
    free(write_zone_data);
    exit(1);
  }

  for (int i = 0; i < BUFFER_SIZE; i++)
    write_zone_data[i] = '\0'; // Init to all nulls
  for (int i = 0; i < read_zone_size; i++)
    read_zone_data[i] = '\0'; // Init to all nulls

  printf("%s\n", read_zone_data);
  printf("%s\n", read_zone_data + 65);
  printf("%s\n", read_zone_data + 130);

  cursor_position = 0;

  /* Start the network thread */
  if (pthread_create(&network_thread, NULL, network_thread_f, NULL) != 0) {
    fprintf(stderr, "Error: pthread_create() failed for network thread.\n");
    exit(1);
  }

  /* Start the drawing thread */
  if (pthread_create(&drawing_thread, NULL, drawing_thread_f, NULL) != 0) {
    fprintf(stderr, "Error: pthread_create() failed for drawing thread.\n");
    exit(1);
  }

  /* Look for and handle keypresses */
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *)&packet, sizeof(packet),
                              &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
              packet.keycode[1]);

      pthread_mutex_lock(&write_zone_mutex);

      // Update write_zone_data according to keyboard input here
      char input = decode_keypress(packet.keycode);
      // If input is a normal ascii character to add, add it to write_zone_data
      // at index cursor_position

      // If input is something we agree means a backspace, remove the character
      // before cursor_position
      // If the input is something we agree means left arrow, cursor_position--
      // if the input is something we agree means right arrow, cursor_position++
      // Don't let cursor position get away from the last character, become
      // negative, or let write_zone_data exceed BUFFER_SIZE.
      // write_zone_data[BUFFER_SIZE - 1] should ALWAYS be '\0' - don't let them
      // overwrite
      if (DEBUG)
        printf("%s (%c)\n", keystate, input);

      pthread_mutex_unlock(&write_zone_mutex);
      // fbputs(keystate, 6, 0);
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
        break;
      }
    }
  }

  /* Terminate the threads */
  pthread_cancel(network_thread);
  drawing_thread_terminate = 1;
  pthread_cancel(drawing_thread);

  /* Wait for the threads to finish */
  pthread_join(network_thread, NULL);
  pthread_join(drawing_thread, NULL);

  /* Free allocated memory */
  free(write_zone_data);
  free(read_zone_data);

  return 0;
}

/*
 * Runs a layout initialization: sets a dividing line down the third row from
 * the bottom
 */
void draw_layout() {
  for (int i = 0; i < TEXT_COLS_ON_SCREEN; i++)
    fbputchar(DIVIDING_LINE, TEXT_ROWS_ON_SCREEN - 3, i);
}

void *drawing_thread_f(void *ignored) {
  while (!drawing_thread_terminate) {
    /* Draw dividing line */
    draw_layout();

    // Now to split the data from the write zone into lines that will fit
    char write_r_1[TEXT_COLS_ON_SCREEN + 1], write_r_2[TEXT_COLS_ON_SCREEN + 1];
    write_r_1[TEXT_COLS_ON_SCREEN] = '\0';
    write_r_2[TEXT_COLS_ON_SCREEN] = '\0';

    pthread_mutex_lock(&write_zone_mutex);
    // Compute string length
    int write_str_length;
    char *first_null = strchr(write_zone_data, '\0');
    write_str_length = (first_null == NULL) ? 0 : first_null - write_zone_data;

    // How many rows would it take to fit this on the screen?
    // We lose a char to >
    int write_num_rows = (write_str_length) / 64 + 1;
    int first_char_displayed; // The index of the first visible char

    if (write_num_rows <= 2)
      first_char_displayed = 0; // Display from the start
    else
      first_char_displayed = (write_num_rows - 2) * TEXT_COLS_ON_SCREEN - 1;

    // Now fill r_1 and r_2 starting from first_char_displayed. Add > if needed
    if (first_char_displayed == 0) {
      write_r_1[0] = '>';
      strncpy(write_r_1 + 1, write_zone_data, TEXT_COLS_ON_SCREEN - 1);
    } else
      strncpy(write_r_1, write_zone_data + first_char_displayed,
              TEXT_COLS_ON_SCREEN);

    if (write_num_rows > 1)
      strncpy(write_r_2,
              write_zone_data + (write_num_rows - 1) * TEXT_COLS_ON_SCREEN - 1,
              TEXT_COLS_ON_SCREEN);

    if (DEBUG && write_str_length > 0) {
      printf("Data in write zone: %s\n", write_zone_data);
      printf("Data length: %d\n", write_str_length);
      printf("Rows required: %d\n", write_num_rows);
      printf("First char displayed: %d (%c)\n", first_char_displayed,
             write_zone_data[first_char_displayed]);
    }

    pthread_mutex_unlock(&write_zone_mutex);

    // Now draw the parts that fit in our two lines
    fbputs(write_r_1, TEXT_ROWS_ON_SCREEN - 2, 0);
    fbputs(write_r_2, TEXT_ROWS_ON_SCREEN - 1, 0);

    // Drawing data from the read zone. This is done a lot more simply than the
    // write zone. We let read_zone_data be cols - 3 rows of null-terminated
    // lines of text, and draw them in that order. We let the networking code
    // actually handle the splitting
    for (int i = 0; i < TEXT_ROWS_ON_SCREEN - 3; i++)
      fbputs(read_zone_data + i * (TEXT_COLS_ON_SCREEN + 1), i, 0);
  }
  return NULL;
}

void *network_thread_f(void *ignored) {
  char recvBuf[BUFFER_SIZE];
  char messages[BUFFER_SIZE][TEXT_ROWS_ON_SCREEN - 3];

  int n;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
  }

  return NULL;
}

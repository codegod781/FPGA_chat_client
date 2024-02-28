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
#define MAP_SIZE 128

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

char *key_map[MAP_SIZE];

void init_keymap() {
  // Initialize all elements of the keymap to NULL
  for (int i = 0; i < MAP_SIZE; i++) {
    key_map[i] = NULL;
  }

  key_map[0x04] = "a"; // Keyboard a and A
  key_map[0x05] = "b"; // Keyboard b and B
  key_map[0x06] = "c"; // Keyboard c and C
  key_map[0x07] = "d"; // Keyboard d and D
  key_map[0x08] = "e"; // Keyboard e and E
  key_map[0x09] = "f"; // Keyboard f and F
  key_map[0x0a] = "g"; // Keyboard g and G
  key_map[0x0b] = "h"; // Keyboard h and H
  key_map[0x0c] = "i"; // Keyboard i and I
  key_map[0x0d] = "j"; // Keyboard j and J
  key_map[0x0e] = "k"; // Keyboard k and K
  key_map[0x0f] = "l"; // Keyboard l and L
  key_map[0x10] = "m"; // Keyboard m and M
  key_map[0x11] = "n"; // Keyboard n and N
  key_map[0x12] = "o"; // Keyboard o and O
  key_map[0x13] = "p"; // Keyboard p and P
  key_map[0x14] = "q"; // Keyboard q and Q
  key_map[0x15] = "r"; // Keyboard r and R
  key_map[0x16] = "s"; // Keyboard s and S
  key_map[0x17] = "t"; // Keyboard t and T
  key_map[0x18] = "u"; // Keyboard u and U
  key_map[0x19] = "v"; // Keyboard v and V
  key_map[0x1a] = "w"; // Keyboard w and W
  key_map[0x1b] = "x"; // Keyboard x and X
  key_map[0x1c] = "y"; // Keyboard y and Y
  key_map[0x1d] = "z"; // Keyboard z and Z

  key_map[0x1e] = "1"; // Keyboard 1 and !
  key_map[0x1f] = "2"; // Keyboard 2 and @
  key_map[0x20] = "3"; // Keyboard 3 and #
  key_map[0x21] = "4"; // Keyboard 4 and $
  key_map[0x22] = "5"; // Keyboard 5 and %
  key_map[0x23] = "6"; // Keyboard 6 and ^
  key_map[0x24] = "7"; // Keyboard 7 and &
  key_map[0x25] = "8"; // Keyboard 8 and *
  key_map[0x26] = "9"; // Keyboard 9 and (
  key_map[0x27] = "0"; // Keyboard 0 and )

  key_map[0x28] = "ENTER";     // Keyboard Return (ENTER)
  key_map[0x29] = "ESC";       // Keyboard ESCAPE
  key_map[0x2a] = "BACKSPACE"; // Keyboard DELETE (Backspace)
  key_map[0x2b] = "TAB";       // Keyboard Tab
  key_map[0x2c] = "SPACE";     // Keyboard Spacebar
  key_map[0x2d] = "-";         // Keyboard - and _
  key_map[0x2e] = "=";         // Keyboard = and +
  key_map[0x2f] = "[";         // Keyboard [ and {
  key_map[0x30] = "]";         // Keyboard ] and }
  key_map[0x31] = "\\";        // Keyboard \ and |
  key_map[0x32] = "#";         // Keyboard Non-US # and ~
  key_map[0x33] = ";";         // Keyboard ; and :
  key_map[0x34] = "'";         // Keyboard ' and "
  key_map[0x35] = "`";         // Keyboard ` and ~
  key_map[0x36] = ",";         // Keyboard , and <
  key_map[0x37] = ".";         // Keyboard . and >
  key_map[0x38] = "/";         // Keyboard / and ?
  key_map[0x39] = "CAPSLOCK";  // Keyboard Caps Lock

  key_map[0x3a] = "F1";  // Keyboard F1
  key_map[0x3b] = "F2";  // Keyboard F2
  key_map[0x3c] = "F3";  // Keyboard F3
  key_map[0x3d] = "F4";  // Keyboard F4
  key_map[0x3e] = "F5";  // Keyboard F5
  key_map[0x3f] = "F6";  // Keyboard F6
  key_map[0x40] = "F7";  // Keyboard F7
  key_map[0x41] = "F8";  // Keyboard F8
  key_map[0x42] = "F9";  // Keyboard F9
  key_map[0x43] = "F10"; // Keyboard F10
  key_map[0x44] = "F11"; // Keyboard F11
  key_map[0x45] = "F12"; // Keyboard F12

  key_map[0x46] = "SYSRQ";      // Keyboard Print Screen
  key_map[0x47] = "SCROLLLOCK"; // Keyboard Scroll Lock
  key_map[0x48] = "PAUSE";      // Keyboard Pause
  key_map[0x49] = "INSERT";     // Keyboard Insert
  key_map[0x4a] = "HOME";       // Keyboard Home
  key_map[0x4b] = "PAGEUP";     // Keyboard Page Up
  key_map[0x4c] = "DELETE";     // Keyboard Delete Forward
  key_map[0x4d] = "END";        // Keyboard End
  key_map[0x4e] = "PAGEDOWN";   // Keyboard Page Down
  key_map[0x4f] = "RIGHT";      // Keyboard Right Arrow
  key_map[0x50] = "LEFT";       // Keyboard Left Arrow
  key_map[0x51] = "DOWN";       // Keyboard Down Arrow
  key_map[0x52] = "UP";         // Keyboard Up Arrow

  key_map[0x53] = "NUMLOCK";    // Keyboard Num Lock and Clear
  key_map[0x54] = "KPSLASH";    // Keypad /
  key_map[0x55] = "KPASTERISK"; // Keypad *
  key_map[0x56] = "KPMINUS";    // Keypad -
  key_map[0x57] = "KPPLUS";     // Keypad +
  key_map[0x58] = "KPENTER";    // Keypad ENTER
  key_map[0x59] = "KP1";        // Keypad 1 and End
  key_map[0x5a] = "KP2";        // Keypad 2 and Down Arrow
  key_map[0x5b] = "KP3";        // Keypad 3 and PageDn
  key_map[0x5c] = "KP4";        // Keypad 4 and Left Arrow
  key_map[0x5d] = "KP5";        // Keypad 5
  key_map[0x5e] = "KP6";        // Keypad 6 and Right Arrow
  key_map[0x5f] = "KP7";        // Keypad 7 and Home
  key_map[0x60] = "KP8";        // Keypad 8 and Up Arrow
  key_map[0x61] = "KP9";        // Keypad 9 and Page Up
  key_map[0x62] = "KP0";        // Keypad 0 and Insert
  key_map[0x63] = "KPDOT";      // Keypad . and Delete

  key_map[0x64] = "102ND";   // Keyboard Non-US \ and |
  key_map[0x65] = "COMPOSE"; // Keyboard Application
  key_map[0x66] = "POWER";   // Keyboard Power
  key_map[0x67] = "KPEQUAL"; // Keypad =
}

char *decode_keypress(int *keycode) {
  int usb_code = (int)keycode[0]; // Simulating 'a' key
  printf("The first value stored in the list: %d\n", usb_code);
  char *output = key_map[usb_code];
  printf("Character: %s\n", output);
  return output;
}

void write_char(char *input, int cursor) {
  int size = strlen(write_zone_data);

  if (size < BUFFER_SIZE && cursor < BUFFER_SIZE) {
    if (strcmp(input, "BACKSPACE") == 0) {
      delete_char(cursor);
    } else if (strcmp(input, "LEFT") == 0) {
      move_cursor_left(cursor);
    } else if (strcmp(input, "RIGHT") == 0) {
      move_cursor_right(cursor);
    } else {
      write_zone_data[cursor] = *input;
      cursor++;
    }
  } else {
    fprintf(stderr, "Buffer Overflow\n");
  }
}

void delete_char(int cursor) {
  if (cursor > 0) { // Make sure cursor is not at the beginning
    // Shift characters to the left starting from the cursor position
    for (int i = cursor - 1; i < BUFFER_SIZE - 1; i++) {
      write_zone_data[i] = write_zone_data[i + 1];
      // Stop shifting when we encounter the null terminator
      if (write_zone_data[i] == '\0') {
        break;
      }
    }
    (cursor)--; // Move cursor one position to the left
  }
}

void move_cursor_left(int cursor) {
  if (cursor > 0) {
    cursor--; // Move cursor one position to the left
  }
}

void move_cursor_right(int cursor) {
  if (write_zone_data[cursor] != '\0') {
    cursor++; // Move cursor one position to the right
  }
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
      (TEXT_ROWS_ON_SCREEN - 3) * (TEXT_COLS_ON_SCREEN + 1) * sizeof(char);
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

  init_keymap();

  int *ptr;

  // Allocate memory for the integer array
  ptr = (int *)malloc(3 * sizeof(int));

  // Check if memory allocation was successful
  if (ptr == NULL) {
    printf("Memory allocation failed\n");
    return 1; // Exit with error
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
      char *input = decode_keypress(packet.keycode);

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
      write_char(input, cursor_position);

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
  // Where we will stored messages before processing them into read_zone_data
  char messages[TEXT_ROWS_ON_SCREEN - 3][BUFFER_SIZE];

  int n, num_messages = 0;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[n] = '\0';

    if (DEBUG)
      printf("Got message: %s\n", recvBuf);

    if (num_messages < TEXT_ROWS_ON_SCREEN - 3)
      strncpy(messages[num_messages++], recvBuf, BUFFER_SIZE - 1);
    else {
      // Shift everything up one and delete the oldest message
      for (int i = 0; i < TEXT_ROWS_ON_SCREEN - 4; i++)
        strncpy(messages[i], messages[i + 1], BUFFER_SIZE - 1);

      // Add new message
      strncpy(messages[TEXT_ROWS_ON_SCREEN - 4], recvBuf, BUFFER_SIZE - 1);
    }

    if (DEBUG) {
      printf("message cache:\n");
      for (int i = 0; i < num_messages; i++) {
        printf("index %d: %s\n", i, messages[i]);
      }
    }
  }

  return NULL;
}

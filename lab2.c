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

#define WRITE_SIZE 300 // Message size limit
#define BUFFER_SIZE 128
#define MAP_SIZE 128

#define DIVIDING_LINE '-'
#define ENTER (char)128
#define ESC (char)129
#define BACKSPACE (char)130
#define SPACE (char)131
#define RIGHT (char)132
#define LEFT (char)133

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
int cursor_position, prev_num_lines, cursor_idx_on_screen;
char *write_zone_data, *read_zone_data;
screen_info screen;

// Debug mode (adds some extra logs)
#define DEBUG 0
// How many rows of text we can fit on screen using fbputs
int TEXT_ROWS_ON_SCREEN;
// How many cols of text we can fit on screen using fpbuts
int TEXT_COLS_ON_SCREEN;
// The size of read_zone_data. To be calculated after the above are populated
uint read_zone_size;

char key_map[MAP_SIZE];

void init_keymap() {
  // Initialize all elements of the keymap to NULL
  for (int i = 0; i < MAP_SIZE; i++) {
    key_map[i] = NULL;
  }

  key_map[0x04] = 'a'; // Keyboard a and A
  key_map[0x05] = 'b'; // Keyboard b and B
  key_map[0x06] = 'c'; // Keyboard c and C
  key_map[0x07] = 'd'; // Keyboard d and D
  key_map[0x08] = 'e'; // Keyboard e and E"
  key_map[0x09] = 'f'; // Keyboard f and F"
  key_map[0x0a] = 'g'; // Keyboard g and G"
  key_map[0x0b] = 'h'; // Keyboard h and H"
  key_map[0x0c] = 'i'; // Keyboard i and I"
  key_map[0x0d] = 'j'; // Keyboard j and J"
  key_map[0x0e] = 'k'; // Keyboard k and K"
  key_map[0x0f] = 'l'; // Keyboard l and L"
  key_map[0x10] = 'm'; // Keyboard m and M"
  key_map[0x11] = 'n'; // Keyboard n and N
  key_map[0x12] = 'o'; // Keyboard o and O
  key_map[0x13] = 'p'; // Keyboard p and P
  key_map[0x14] = 'q'; // Keyboard q and Q
  key_map[0x15] = 'r'; // Keyboard r and R
  key_map[0x16] = 's'; // Keyboard s and S
  key_map[0x17] = 't'; // Keyboard t and T
  key_map[0x18] = 'u'; // Keyboard u and U
  key_map[0x19] = 'v'; // Keyboard v and V
  key_map[0x1a] = 'w'; // Keyboard w and W
  key_map[0x1b] = 'x'; // Keyboard x and X
  key_map[0x1c] = 'y'; // Keyboard y and Y
  key_map[0x1d] = 'z'; // Keyboard z and Z

  key_map[0x1e] = '1'; // Keyboard 1 and !
  key_map[0x1f] = '2'; // Keyboard 2 and @
  key_map[0x20] = '3'; // Keyboard 3 and #
  key_map[0x21] = '4'; // Keyboard 4 and $
  key_map[0x22] = '5'; // Keyboard 5 and %
  key_map[0x23] = '6'; // Keyboard 6 and ^
  key_map[0x24] = '7'; // Keyboard 7 and &
  key_map[0x25] = '8'; // Keyboard 8 and *
  key_map[0x26] = '9'; // Keyboard 9 and (
  key_map[0x27] = '0'; // Keyboard 0 and )

  key_map[0x28] = ENTER;     // Keyboard Return (ENTER)
  key_map[0x29] = ESC;       // Keyboard ESCAPE
  key_map[0x2a] = BACKSPACE; // Keyboard DELETE (Backspace)
  key_map[0x2c] = ' ';       // Keyboard Spacebar
  key_map[0x2d] = '-';       // Keyboard - and _
  key_map[0x2e] = '=';       // Keyboard = and +
  key_map[0x2f] = '[';       // Keyboard [ and {
  key_map[0x30] = ']';       // Keyboard ] and }
  key_map[0x31] = '\\';      // Keyboard \ and |
  key_map[0x32] = '#';       // Keyboard Non-US # and ~
  key_map[0x33] = ';';       // Keyboard ; and :
  key_map[0x34] = '\'';      // Keyboard ' and '
  key_map[0x35] = '`';       // Keyboard ` and ~
  key_map[0x36] = ',';       // Keyboard , and <
  key_map[0x37] = '.';       // Keyboard . and >
  key_map[0x38] = '/';       // Keyboard / and ?

  key_map[0x4f] = RIGHT; // Keyboard Right Arrow
  key_map[0x50] = LEFT;  // Keyboard Left Arrow

  key_map[0x6d] = '_';       // Keyboard - and _
  key_map[0x6e] = '+';       // Keyboard = and +
  key_map[0x6f] = '{';       // Keyboard [ and {
  key_map[0x70] = '}';       // Keyboard ] and }
  key_map[0x71] = '|';      // Keyboard \ and |
  key_map[0x72] = '#';       // Keyboard Non-US # and ~
  key_map[0x73] = ':';       // Keyboard ; and :
  key_map[0x54] = '\'';      // Keyboard ' and '
  key_map[0x75] = '~';       // Keyboard ` and ~
  key_map[0x76] = '<';       // Keyboard , and <
  key_map[0x77] = '>';       // Keyboard . and >
  key_map[0x78] = '?';       // Keyboard / and ?


}

char decode_keypress(uint8_t *keycode, uint8_t modifiers) {
  if (keycode[0] != 0 && keycode[1] != 0)
    return NULL; // Skip if two pressed at once

  char output = key_map[(int)keycode[0]];

  if (modifiers == 0x20 || modifiers == 0x02 || modifiers == 0x22)
    if(*keycode[0] >= 0x2d && *keycode[0] <= 0x38) {
        // Add 0x40 to the pointer value
        *keycode[0] += 0x40;
        char output = key_map[(int)keycode[0]];
    } 
    else{
      output = toupper(output);
    }
    

  return output;
}

void write_char(char input) {
  if (input == NULL)
    return;

  int size = strlen(write_zone_data);

  if (DEBUG) {
    printf("Size: %d\n", size);
    printf("Got keypress: %c\n", input);
    printf("Cursor: %d\n", cursor_position);
    printf("Write string: %s\n", write_zone_data);
  }

  if (size < WRITE_SIZE && cursor_position < WRITE_SIZE - 1) {
    if (input == BACKSPACE && size > 0) {
      delete_char();
      if (DEBUG)
        printf("String after delete: %s\n", write_zone_data);
    } else if (input == LEFT) {
      move_cursor_left();
    } else if (input == RIGHT) {
      move_cursor_right();
    } else if (input < ENTER || input > LEFT && input != NULL) {
      write_zone_data[cursor_position] = input;
      cursor_position++;
    }
  } else {
    cursor_position = WRITE_SIZE - 2;
    fprintf(stderr, "Buffer Overflow\n");
  }
}

void delete_char() {
  if (cursor_position > 0) { // Make sure cursor is not at the beginning
    // Shift characters to the left starting from the cursor position
    for (int i = cursor_position - 1; i < WRITE_SIZE; i++) {
      write_zone_data[i] = write_zone_data[i + 1];
      // Stop shifting when we encounter the null terminator
      if (write_zone_data[i] == '\0') {
        break;
      }
    }
    cursor_position--; // Move cursor one position to the left
    fbclear();
  }
}

void move_cursor_left() {
  if (cursor_position > 0 && cursor_idx_on_screen > 0)
    cursor_position--; // Move cursor one position to the left
}

void move_cursor_right() {
  if (write_zone_data[cursor_position] != '\0')
    cursor_position++; // Move cursor one position to the right
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

  // We will allow the character to enter up to WRITE_SIZE characters
  // (including the \0) of write data in the writing panel on the bottom, as
  // that's how much data we'll allow the user to send on the socket
  if ((write_zone_data = malloc(WRITE_SIZE * sizeof(char))) == NULL) {
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

  for (int i = 0; i < WRITE_SIZE; i++)
    write_zone_data[i] = '\0'; // Init to all nulls
  for (int i = 0; i < read_zone_size; i++)
    read_zone_data[i] = '\0'; // Init to all nulls

  printf("%s\n", read_zone_data);
  printf("%s\n", read_zone_data + 65);
  printf("%s\n", read_zone_data + 130);

  cursor_position = 0;
  cursor_idx_on_screen = 0; // Used to position the cursor across pages
  // How many lines were used in the write zone at the last keypress
  prev_num_lines = 0;

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

  /* Look for and handle keypresses */
  for (;;) {
    int result = libusb_interrupt_transfer(keyboard, endpoint_address,
                                           (unsigned char *)&packet,
                                           sizeof(packet), &transferred, 250);
    if (result == LIBUSB_ERROR_TIMEOUT) {
      // Pressing and holding (or holding nothing)
      pthread_mutex_lock(&write_zone_mutex);
      char input = decode_keypress(packet.keycode, packet.modifiers);
      write_char(input);
      pthread_mutex_unlock(&write_zone_mutex);
      continue;
    } else if (result < 0) {
      fprintf(stderr, "Error: error encountered in libusb read.\n");
      continue;
    }
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
              packet.keycode[1]);

      pthread_mutex_lock(&write_zone_mutex);

      // Update write_zone_data according to keyboard input here
      char input = decode_keypress(packet.keycode, packet.modifiers);

      // If input is a normal ascii character to add, add it to write_zone_data
      // at index cursor_position

      // If input is something we agree means a backspace, remove the character
      // before cursor_position
      // If the input is something we agree means left arrow, cursor_position--
      // if the input is something we agree means right arrow, cursor_position++
      // Don't let cursor position get away from the last character, become
      // negative, or let write_zone_data exceed WRITE_SIZE.
      // write_zone_data[WRITE_SIZE - 1] should ALWAYS be '\0' - don't let them
      // overwrite
      if (input == ENTER) {
        // Send the message to the server. We need to add a carriage return first
        char *message;

        if ((message = malloc(WRITE_SIZE + 2)) == NULL)
          fprintf(stderr, "Error: malloc() error when building message.\n");
        else {
          strncpy(message, write_zone_data, WRITE_SIZE);
          message[strlen(write_zone_data)] = '\r';
          message[strlen(write_zone_data + 1)] = '\n';

          if (write(sockfd, message, strlen(message)) != strlen(message))
            fprintf(stderr,
                    "Error: write() error sending message to server.\n");
          else {
            printf("Message successfully sent!\n");
            fbclear();

            // Clear write_zone_data after sending
            memset(write_zone_data, 0, WRITE_SIZE);
            cursor_position = 0;
          }

          free(message);
        }
      } else
        write_char(input);

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
    fbputchar(DIVIDING_LINE, TEXT_ROWS_ON_SCREEN - 3, i, 0);
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
    else
      strncpy(write_r_2, "\0", TEXT_COLS_ON_SCREEN); // Fill it with 0s

    if (DEBUG && write_str_length > 0) {
      printf("Data in write zone: %s\n", write_zone_data);
      printf("Data length: %d\n", write_str_length);
      printf("Rows required: %d\n", write_num_rows);
      printf("First char displayed: %d (%c)\n", first_char_displayed,
             write_zone_data[first_char_displayed]);
    }

    pthread_mutex_unlock(&write_zone_mutex);

    // Do we need to do a screen clear?
    if (prev_num_lines != write_num_rows) {
      if (DEBUG)
        printf("prev_num_lines (%d) != write_num_rows (%d)\n", prev_num_lines,
               write_num_rows);
      fbclear();
      prev_num_lines = write_num_rows;
    }

    cursor_idx_on_screen = cursor_position - first_char_displayed;
    if (first_char_displayed != 0)
      cursor_idx_on_screen--; // There's no longer the >
    char cursor_char = NULL;

    if (cursor_position > 0 && cursor_position < WRITE_SIZE)
      cursor_char = write_zone_data[cursor_position - 1];

    for (int i = 0; i < 2 * TEXT_COLS_ON_SCREEN; i++) {
      char to_write;
      if (i < TEXT_COLS_ON_SCREEN)
        to_write = write_r_1[i];
      else
        to_write = write_r_2[i - TEXT_COLS_ON_SCREEN];

      // Skip this char if needed
      if (cursor_idx_on_screen == i && !(first_char_displayed == 0 && i == 0))
        to_write = '\0';

      if (to_write != '\0') {
        if (i < TEXT_COLS_ON_SCREEN)
          fbputchar(to_write, TEXT_ROWS_ON_SCREEN - 2, i, 0);
        else
          fbputchar(to_write, TEXT_ROWS_ON_SCREEN - 1, i - TEXT_COLS_ON_SCREEN,
                    0);
      }
    }

    if (strlen(write_zone_data) == 0) {
      fbputchar('_', TEXT_ROWS_ON_SCREEN - 2, 1, 1);
    } else {
      if (cursor_char != '\0')
        fbputchar(cursor_char == ' ' ? '_' : cursor_char,
                  TEXT_ROWS_ON_SCREEN -
                      (2 - (cursor_idx_on_screen / TEXT_COLS_ON_SCREEN)),
                  cursor_idx_on_screen % TEXT_COLS_ON_SCREEN, 1);
    }

    // fprintf(stderr, "First displayed: %d, Cursor position: %d, cursor index:
    // %d, char: %c\n",
    // first_char_displayed, cursor_position, cursor_idx_on_screen,
    // cursor_char);

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

  int n, num_lines_taken = 0;
  /* Receive data */
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[n] = '\0';

    // Replace \r and \n with spaces
    char *next_replace;

    while ((next_replace = strchr(recvBuf, '\n')) != NULL)
      *next_replace = ' ';

    while ((next_replace = strchr(recvBuf, '\r')) != NULL)
      *next_replace = ' ';

    printf("Got message: %s\n", recvBuf);
    int lines_needed = 1;

    if (strlen(recvBuf) > TEXT_COLS_ON_SCREEN)
      lines_needed += strlen(recvBuf) / TEXT_COLS_ON_SCREEN;

    printf("Lines needed: %d, lines taken: %d\n", lines_needed,
           num_lines_taken);

    pthread_mutex_lock(&read_zone_mutex);

    if (num_lines_taken + lines_needed <= TEXT_ROWS_ON_SCREEN - 3) {
      // No shifting needed
      for (int i = num_lines_taken; i < num_lines_taken + lines_needed; i++) {
        strncpy(read_zone_data + i * (TEXT_COLS_ON_SCREEN + 1), recvBuf + (i - num_lines_taken) * TEXT_COLS_ON_SCREEN, TEXT_COLS_ON_SCREEN);
      }

      num_lines_taken += lines_needed;
    }

    pthread_mutex_unlock(&read_zone_mutex);
  }

  return NULL;
}

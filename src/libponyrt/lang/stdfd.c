#include <platform.h>
#include <stdio.h>
#include <string.h>
#include "../asio/asio.h"

#ifndef PLATFORM_IS_WINDOWS
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#endif

PONY_EXTERN_C_BEGIN

PONY_API FILE* pony_os_stdout()
{
  return stdout;
}

PONY_API FILE* pony_os_stderr()
{
  return stderr;
}

static bool is_stdout_tty = false;
static bool is_stderr_tty = false;

#ifdef PLATFORM_IS_WINDOWS

#ifndef FOREGROUND_MASK
# define FOREGROUND_MASK \
  (FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY)
#endif

#ifndef BACKGROUND_MASK
# define BACKGROUND_MASK \
  (BACKGROUND_RED|BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_INTENSITY)
#endif

static bool is_stdin_tty = false;
static WORD stdout_reset;
static WORD stderr_reset;

static void add_modifier(char* buffer, int* len, DWORD mod)
{
  bool alt = ((mod & LEFT_ALT_PRESSED) > 0) ||
    ((mod & RIGHT_ALT_PRESSED) > 0);

  bool ctrl = ((mod & LEFT_CTRL_PRESSED) > 0) ||
    ((mod & RIGHT_CTRL_PRESSED) > 0);

  bool shift = (mod & SHIFT_PRESSED) > 0;

  if(!alt && !ctrl && !shift)
    return;

  int next_len = *len;
  buffer[next_len++] = ';';

  if(shift)
  {
    if(ctrl)
    {
      if(alt)
        buffer[next_len++] = '8';
      else
        buffer[next_len++] = '6';
    } else if(alt) {
      buffer[next_len++] = '4';
    } else {
      buffer[next_len++] = '2';
    }
  } else if(alt) {
    if(ctrl)
      buffer[next_len++] = '7';
    else
      buffer[next_len++] = '3';
  } else if(ctrl) {
    buffer[next_len++] = '5';
  }

  *len = next_len;
}

static int add_ansi_code(char* buffer, const char* pre, const char* post,
  DWORD mod)
{
  int prelen = (int)strlen(pre);
  memcpy(buffer, pre, prelen);
  add_modifier(buffer, &prelen, mod);

  int postlen = (int)strlen(post);
  memcpy(&buffer[prelen], post, postlen);

  return prelen + postlen;
}

static bool add_input_record(char* buffer, size_t space, size_t *len,
  INPUT_RECORD* rec)
{
  // Returns true if the record can be consumed.
  if(rec->EventType != KEY_EVENT ||
    rec->Event.KeyEvent.bKeyDown == FALSE)
  {
    return true;
  }

  // This is a key down event. Handle key repeats.
  DWORD mod = rec->Event.KeyEvent.dwControlKeyState;
  size_t next_len = *len;
  char out[8];
  int outlen;

  switch(rec->Event.KeyEvent.wVirtualKeyCode)
  {
    case VK_PRIOR: // page up
      outlen = add_ansi_code(out, "\x1B[5", "~", mod);
      break;

    case VK_NEXT: // page down
      outlen = add_ansi_code(out, "\x1B[6", "~", mod);
      break;

    case VK_END:
      outlen = add_ansi_code(out, "\x1B[4", "~", mod);
      break;

    case VK_HOME:
      outlen = add_ansi_code(out, "\x1B[1", "~", mod);
      break;

    case VK_LEFT:
      outlen = add_ansi_code(out, "\x1B[1", "D", mod);
      break;

    case VK_RIGHT:
      outlen = add_ansi_code(out, "\x1B[1", "C", mod);
      break;

    case VK_UP:
      outlen = add_ansi_code(out, "\x1B[1", "A", mod);
      break;

    case VK_DOWN:
      outlen = add_ansi_code(out, "\x1B[1", "B", mod);
      break;

    case VK_INSERT:
      outlen = add_ansi_code(out, "\x1B[2", "~", mod);
      break;

    case VK_DELETE:
      outlen = add_ansi_code(out, "\x1B[3", "~", mod);
      break;

    case VK_F1:
      outlen = add_ansi_code(out, "\x1B[11", "~", mod);
      break;

    case VK_F2:
      outlen = add_ansi_code(out, "\x1B[12", "~", mod);
      break;

    case VK_F3:
      outlen = add_ansi_code(out, "\x1B[13", "~", mod);
      break;

    case VK_F4:
      outlen = add_ansi_code(out, "\x1B[14", "~", mod);
      break;

    case VK_F5:
      outlen = add_ansi_code(out, "\x1B[15", "~", mod);
      break;

    case VK_F6:
      outlen = add_ansi_code(out, "\x1B[17", "~", mod);
      break;

    case VK_F7:
      outlen = add_ansi_code(out, "\x1B[18", "~", mod);
      break;

    case VK_F8:
      outlen = add_ansi_code(out, "\x1B[19", "~", mod);
      break;

    case VK_F9:
      outlen = add_ansi_code(out, "\x1B[20", "~", mod);
      break;

    case VK_F10:
      outlen = add_ansi_code(out, "\x1B[21", "~", mod);
      break;

    case VK_F11:
      outlen = add_ansi_code(out, "\x1B[23", "~", mod);
      break;

    case VK_F12:
      outlen = add_ansi_code(out, "\x1B[24", "~", mod);
      break;

    default:
    {
      // WCHAR to UTF-8
      if(rec->Event.KeyEvent.uChar.UnicodeChar != 0)
      {
        outlen = WideCharToMultiByte(CP_UTF8, 0,
          &rec->Event.KeyEvent.uChar.UnicodeChar, 1, out, 4,
          NULL, NULL);
      }
      else
      {
        return true;
      }
    }
  }

  WORD count = rec->Event.KeyEvent.wRepeatCount;

  if((next_len + (count * outlen)) > space)
    return false;

  for(WORD i = 0; i < count; i++)
  {
    memcpy(&buffer[next_len], out, outlen);
    next_len += outlen;
  }

  *len = next_len;
  return true;
}

static WORD get_attrib(HANDLE handle)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(handle, &csbi);
  return csbi.wAttributes;
}

static COORD get_coord(HANDLE handle, CONSOLE_SCREEN_BUFFER_INFO* csbi)
{
  GetConsoleScreenBufferInfo(handle, csbi);
  return csbi->dwCursorPosition;
}

static void set_coord(HANDLE handle, CONSOLE_SCREEN_BUFFER_INFO* csbi,
  COORD coord)
{
  if(coord.X < csbi->srWindow.Left)
    coord.X = csbi->srWindow.Left;
  else if(coord.X > csbi->srWindow.Right)
    coord.X = csbi->srWindow.Right;

  if(coord.Y < csbi->srWindow.Top)
    coord.Y = csbi->srWindow.Top;
  else if(coord.Y > csbi->srWindow.Bottom)
    coord.Y = csbi->srWindow.Bottom;

  SetConsoleCursorPosition(handle, coord);
}

#else

static struct termios orig_termios;

typedef enum
{
  FD_TYPE_NONE = 0,
  FD_TYPE_DEVICE,
  FD_TYPE_TTY,
  FD_TYPE_PIPE,
  FD_TYPE_FILE
} fd_type_t;

static fd_type_t fd_type(int fd)
{
  fd_type_t type = FD_TYPE_NONE;
  struct stat st;

  if(fstat(fd, &st) != -1)
  {
    switch(st.st_mode & S_IFMT)
    {
      case S_IFIFO:
      case S_IFSOCK:
        // A pipe or a socket.
        type = FD_TYPE_PIPE;
        break;

      case S_IFCHR:
        // A tty or a character device.
        if(isatty(fd))
          type = FD_TYPE_TTY;
        else
          type = FD_TYPE_DEVICE;
        break;

      case S_IFREG:
        // A redirected file.
        type = FD_TYPE_FILE;
        break;

      default:
        // A directory or a block device.
        break;
    }
  }

  return type;
}

static void stdin_tty_restore()
{
  tcsetattr(0, TCSAFLUSH, &orig_termios);
}

static void stdin_tty()
{
  // Turn off canonical mode if we're reading from a tty.
  if(tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    return;

  struct termios io = orig_termios;

  io.c_iflag &= ~(BRKINT | INPCK | ISTRIP | IXON);
  io.c_cflag |= (CS8);
  io.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  io.c_cc[VMIN] = 1;
  io.c_cc[VTIME] = 0;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &io);
  atexit(stdin_tty_restore);
}
#endif

char ansi_parse(const char* buffer, size_t* pos, size_t len,
  int* argc, int* argv)
{
  size_t n;
  int arg = -1;
  char code = -1;

  for(n = *pos; n < len; n++)
  {
    char c = buffer[n];

    if((c >= '0') && (c <= '9'))
    {
      if(arg < 0)
        arg = 0;

      argv[arg] = (argv[arg] * 10) + (c - '0');
    } else if(c == ';') {
      arg = arg + 1;

      if(arg > 5)
        break;
    } else {
      code = c;
      break;
    }
  }

  *pos = n + 1;
  *argc = arg + 1;

  return code;
}

PONY_API void pony_os_stdout_setup()
{
#ifdef PLATFORM_IS_WINDOWS
  HANDLE handle;
  DWORD type;

  handle = GetStdHandle(STD_OUTPUT_HANDLE);
  type = GetFileType(handle);

  DWORD mode;
  if (GetConsoleMode(handle, &mode))
  {
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(handle, &csbi);
  stdout_reset = csbi.wAttributes;
  is_stdout_tty = (type == FILE_TYPE_CHAR);

  handle = GetStdHandle(STD_ERROR_HANDLE);
  type = GetFileType(handle);
  if (GetConsoleMode(handle, &mode))
  {
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }

  GetConsoleScreenBufferInfo(handle, &csbi);
  stderr_reset = csbi.wAttributes;
  is_stderr_tty = (type == FILE_TYPE_CHAR);
#else
  is_stdout_tty = (fd_type(STDOUT_FILENO) == FD_TYPE_TTY);
  is_stderr_tty = (fd_type(STDERR_FILENO) == FD_TYPE_TTY);

  // Use unbuffered output if we're writing to a tty, otherwise line buffered.
  if(is_stdout_tty)
    setvbuf(stdout, NULL, _IONBF, 0);
  else
    setvbuf(stdout, NULL, _IOLBF, 0);

  if(is_stderr_tty)
    setvbuf(stderr, NULL, _IONBF, 0);
  else
    setvbuf(stderr, NULL, _IOLBF, 0);
#endif
}

PONY_API bool pony_os_stdin_setup()
{
  // use events by default
  bool stdin_event_based = true;
  // Return true if reading stdin should be event based.
#ifdef PLATFORM_IS_WINDOWS
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD type = GetFileType(handle);

  if(type == FILE_TYPE_CHAR)
  {
    // TTY
    DWORD mode;
    GetConsoleMode(handle, &mode);
    SetConsoleMode(handle,
      mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    is_stdin_tty = true;
  }
#else
  // on *nix only not use events when stdin is a redirected file
  fd_type_t stdin_type = fd_type(STDIN_FILENO);
  if((stdin_type == FD_TYPE_TTY) && is_stdout_tty)
    stdin_tty();
  if(stdin_type == FD_TYPE_FILE)
    stdin_event_based = false;
#endif
  return stdin_event_based;
}

PONY_API size_t pony_os_stdin_read(char* buffer, size_t space)
{
#ifdef PLATFORM_IS_WINDOWS
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  size_t len = 0;

  if(is_stdin_tty)
  {
    // TTY. Peek at the console input.
    INPUT_RECORD record[64];
    DWORD count;

    BOOL r = PeekConsoleInput(handle, record, 64, &count);

    if(r == TRUE)
    {
      DWORD consumed = 0;

      while(consumed < count)
      {
        if(!add_input_record(buffer, space, &len, &record[consumed]))
          break;

        consumed++;
      }

      // Pull as many records as we were able to handle.
      ReadConsoleInput(handle, record, consumed, &count);
    }

    // We have no data, but 0 means EOF, so we return -1 which is try again
    if(len == 0)
      len = -1;
  }
  else
  {
    // Not TTY, ie file or pipe. Just use ReadFile.
    DWORD buf_size = (space <= 0xFFFFFFFF) ? (DWORD)space : 0xFFFFFFFF;
    DWORD actual_len = 0;

    BOOL r = ReadFile(handle, buffer, buf_size, &actual_len, NULL);
    len = actual_len;

    if(r == FALSE && GetLastError() == ERROR_BROKEN_PIPE)  // Broken pipe
      len = 0;
  }

  if(len != 0)  // Start listening to stdin notifications again
    ponyint_iocp_resume_stdin();

  return len;
#else
  struct pollfd pfd;
  pfd.fd = STDIN_FILENO;
  pfd.events = POLLIN;
  pfd.revents = 0;

  int n = poll(&pfd, 1, 0);

  if(n != 1)
    return -1;

  return read(STDIN_FILENO, buffer, space);
#endif
}

PONY_API void pony_os_std_print(FILE* fp, char* buffer, size_t len)
{
  if(len == 0)
    fprintf(fp, "\n");
  else
    fprintf(fp, "%*.*s\n", (int)len, (int)len, buffer);
}

PONY_API void pony_os_std_write(FILE* fp, char* buffer, size_t len)
{
  if(len == 0)
    return;

#ifdef PLATFORM_IS_WINDOWS
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  HANDLE handle = (HANDLE)_get_osfhandle(_fileno(fp));

  size_t last = 0;
  size_t pos = 0;
  DWORD n;

  while(pos < (len - 1))
  {
    if((buffer[pos] == '\x1B') && (buffer[pos + 1] == '['))
    {
      if(pos > last)
      {
        // Write any pending data.
        _fwrite_nolock(&buffer[last], pos - last, 1, fp);
        last = pos;
      }

      int argc = 0;
      int argv[6] = {0};

      pos += 2;
      char code = ansi_parse(buffer, &pos, len, &argc, argv);

      switch(code)
      {
        case 'H':
        {
          // Home.
          if(argc >= 2)
          {
            COORD coord = get_coord(handle, &csbi);
            coord.X = argv[0];
            coord.Y = argv[1];
            set_coord(handle, &csbi, coord);
          } else {
            COORD coord = get_coord(handle, &csbi);
            coord.X = 0;
            coord.Y = csbi.srWindow.Top;
            set_coord(handle, &csbi, coord);
          }
          break;
        }

        case 'J':
        {
          // Clear screen.
          // 3 different clear modes, here we always do #2.
          COORD coord = get_coord(handle, &csbi);
          coord.X = 0;
          coord.Y = csbi.srWindow.Top;

          DWORD count = csbi.dwSize.X *
            (csbi.srWindow.Bottom - csbi.srWindow.Top + 2);

          FillConsoleOutputCharacter(handle, ' ', count, coord, &n);
          FillConsoleOutputAttribute(handle, csbi.wAttributes, count, coord,
            &n);
          set_coord(handle, &csbi, coord);
          break;
        }

        case 'K':
        {
          // Erase to the right edge.
          // 3 different modes, here we do #0.
          COORD coord = get_coord(handle, &csbi);
          DWORD count = csbi.dwSize.X - coord.X;

          FillConsoleOutputCharacter(handle, ' ', count, coord, &n);
          FillConsoleOutputAttribute(handle, csbi.wAttributes, count, coord,
            &n);
          set_coord(handle, &csbi, coord);
          break;
        }

        case 'A':
        {
          // Move argv[0] up.
          if(argc > 0)
          {
            COORD coord = get_coord(handle, &csbi);
            coord.Y -= argv[0];
            set_coord(handle, &csbi, coord);
          }
          break;
        }

        case 'B':
        {
          // Move argv[0] down.
          if(argc > 0)
          {
            COORD coord = get_coord(handle, &csbi);
            coord.Y += argv[0];
            set_coord(handle, &csbi, coord);
          }
          break;
        }

        case 'C':
        {
          // Move argv[0] right.
          if(argc > 0)
          {
            COORD coord = get_coord(handle, &csbi);
            coord.X += argv[0];
            set_coord(handle, &csbi, coord);
          }
          break;
        }

        case 'D':
        {
          // Move argv[0] left.
          if(argc > 0)
          {
            COORD coord = get_coord(handle, &csbi);
            coord.X -= argv[0];
            set_coord(handle, &csbi, coord);
          }
          break;
        }

        case 'm':
        {
          WORD attr = get_attrib(handle);

          for(int i = 0; i < argc; i++)
          {
            int m = argv[i];

            if(m == 0)
            {
              // reset
              attr = ((fp == stdout) ? stdout_reset : stderr_reset);
              break;
            } else if((m == 7) || (m == 27)) {
              // reverse
              attr =
                ((attr & FOREGROUND_MASK) << 4) |
                ((attr & BACKGROUND_MASK) >> 4);
            } else if((m >= 30) && (m <= 37)) {
              // foreground
              attr = attr & BACKGROUND_MASK;
              m = m - 30;

              if(m & 1)
                attr |= FOREGROUND_RED;

              if(m & 2)
                attr |= FOREGROUND_GREEN;

              if(m & 4)
                attr |= FOREGROUND_BLUE;
            } else if((m >= 40) && (m <= 47)) {
              // background
              attr = attr & FOREGROUND_MASK;
              m = m - 40;

              if(m & 1)
                attr |= BACKGROUND_RED;

              if(m & 2)
                attr |= BACKGROUND_GREEN;

              if(m & 4)
                attr |= BACKGROUND_BLUE;
            } else if((m >= 90) && (m <= 97)) {
              // bright foreground
              attr = (attr & BACKGROUND_MASK) | FOREGROUND_INTENSITY;
              m = m - 90;

              if(m & 1)
                attr |= FOREGROUND_RED;

              if(m & 2)
                attr |= FOREGROUND_GREEN;

              if(m & 4)
                attr |= FOREGROUND_BLUE;
            } else if((m >= 100) && (m <= 107)) {
              // bright background
              attr = (attr & FOREGROUND_MASK) | BACKGROUND_INTENSITY;
              m = m - 100;

              if(m & 1)
                attr |= BACKGROUND_RED;

              if(m & 2)
                attr |= BACKGROUND_GREEN;

              if(m & 4)
                attr |= BACKGROUND_BLUE;
            }
          }

          SetConsoleTextAttribute(handle, attr);
          break;
        }

        default:
          // Unrecognised, skip it.
          break;
      }

      last = pos;
    } else {
      pos++;
    }
  }

  // Write any remaining data.
  if(len > last)
    fwrite(&buffer[last], len - last, 1, fp);

#elif defined PLATFORM_IS_LINUX
  fwrite_unlocked(buffer, len, 1, fp);
#else
  fwrite(buffer, len, 1, fp);
#endif
}

PONY_API void pony_os_std_flush(FILE* fp)
{
  // avoid flushing all streams
  if(fp == NULL)
    return;

#ifdef PLATFORM_IS_WINDOWS
  _fflush_nolock(fp);
#elif defined PLATFORM_IS_LINUX
  fflush_unlocked(fp);
#else
  fflush(fp);
#endif
}

#ifdef PLATFORM_IS_WINDOWS
// Unsurprisingly Windows uses a different order for colors. Map them here.
// We use the bright versions here, hence the additional 8 on each value.
static const uint8_t map_color[8] =
{
   0, // black
  12, // red
  10, // green
  14, // yellow
   9, // blue
  13, // magenta
  11, // cyan
  15  // white
};
#endif

PONY_EXTERN_C_END

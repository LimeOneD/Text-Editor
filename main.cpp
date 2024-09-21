////////////////////// till the welcome message
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
/* defines */
#define CTRL_KEY(k) ((k) & 0x1f)
#define VERSION "0.0.1"
/* data */
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios original_termios_mode;
};

struct editorConfig E;
/* terminal */
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios_mode)==-1) die("tcsetattr");

}

void enableRawMode()
{
    if(tcgetattr(STDIN_FILENO, &E.original_termios_mode)==-1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.original_termios_mode;

    tcgetattr(STDIN_FILENO, &raw);
    raw.c_oflag &= ~(OPOST);
    raw.c_iflag &= ~(IXON | ICRNL| BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME]=1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)==-1)die("tcsetattr");
}


char editorReadKey()
{
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1))!= 1)
    {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;

}

int getCursorPosition(int *rows, int *cols)
{
    char buf[32];
    unsigned int i=0;

    if(write(STDOUT_FILENO, "\x1b[6n",4) != 4) return -1;
    while(i<sizeof(buf) -1)
    {
        if(read(STDIN_FILENO, &buf[i], 1)!=1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0]!='\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;


}


int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;
    if( ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) !=12) return -1;
        editorReadKey();
        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
/* append buffer*/
struct abuf
{
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len)
{
    char *new_c = (char*)realloc(ab->b, ab->len + len);

    if(new_c == NULL) return ;
    memcpy(&new_c[ab->len], s, len);
    ab->b = new_c;
    ab->len +=len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/* output */
void editorDrawRows(struct abuf *ab)
{
    int y;
    for(y=0; y<E.screenrows; y++)
    {
        if(y==0)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), 
                    "sym's editor -- version %s", VERSION);
            if(welcomelen > E.screencols) welcomelen = E.screencols;
            abAppend(ab, welcome, welcomelen);
        }
        else
        {
            abAppend(ab, "~", 1);
        }
            abAppend(ab, "\x1b[K", 3);
            if(y<E.screenrows-1)
                abAppend(ab, "\r\n", 2);
    }
    
}
void editorRefreshScreen()
{
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l",6);
    //abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);
    editorDrawRows(&ab);
    
    abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h",6);
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);

}
/* input */
void editorProcessKeypress()
{
    char c = editorReadKey();
    switch (c)
    {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;
    }
}

/*init */
void initEditor()
{
    if(getWindowSize(&E.screenrows, &E.screencols)==-1)
        die("getWindowSize");
}

int main()
{
    enableRawMode();
    initEditor();
    /* in place of STDIN_RILENO we can use 1 as 1specify the file descriptor which is the terminal*/
    while(1)
    {
        editorRefreshScreen();
        editorProcessKeypress();   
    }

    return 0;
}

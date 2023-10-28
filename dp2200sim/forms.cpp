/*
 * Simple ncurses form example with fields that actually behaves like fields.
 *
 * How to run:
 *	cc -Wall -Werror -g -pedantic -o test forms.c -lform -lncurses
 */
#include <curses.h>
#include <form.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>
#include <time.h>

static FORM *form;
static WINDOW *win_body, *win_form;
FILE * logfile;

typedef struct m {
  int data;
  int address;
  int ascii;
} M;

std::vector<FIELD *> fields;
std::vector<FIELD *> addressFields;
std::vector<FIELD *> dataFields;
std::vector<M> indexToAddress;
std::vector<FIELD *> asciiFields;

unsigned char memory[16384];
int base=0;

void updateForm(int startAddress) {
  int i, k=0,j;
  char b[5];
  unsigned char t;
  char asciiB[17], fieldB[19];
  for (i=0; i<16 && startAddress < 0x4000; startAddress+=16,i++) {
    snprintf(b,5,"%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (j=0;j<16; j++) {
      t=memory[startAddress+j];
      snprintf(b,3,"%02X", t);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j]=(t>=0x20 && t<127)?t:'.';
    }
    asciiB[j]=0;
    snprintf(fieldB, 19, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
  for (; i<16; startAddress+=16,i++) {
    sprintf(b,"%04X", startAddress);
    set_field_buffer(addressFields[i], 0, b);
    for (int j=0;j<16; j++) {
      t=memory[startAddress+j];
      snprintf(b,3,"%02X", memory[startAddress+j]);
      set_field_buffer(dataFields[k], 0, b);
      k++;
      asciiB[j]=(t>=0x20 && t<127)?t:'.';
    }
    asciiB[j]=0;
    snprintf(fieldB, 19, "|%s|", asciiB);
    set_field_buffer(asciiFields[i], 0, fieldB);
  }
}



static void driver(int ch)
{
	int i;

	switch (ch) {
    case 27: // ESC 
      updateForm(base);
      break;
		case KEY_DOWN:
			form_driver(form, REQ_DOWN_FIELD);
			form_driver(form, REQ_BEG_FIELD);
			break;

		case KEY_UP:
			form_driver(form, REQ_UP_FIELD);
			form_driver(form, REQ_BEG_FIELD);
			break;

		case KEY_LEFT:
			form_driver(form, REQ_LEFT_FIELD);
			break;

		case KEY_RIGHT:
			form_driver(form, REQ_RIGHT_FIELD);
			break;

		// Delete the char before cursor
		case KEY_BACKSPACE:
		case 127:
			form_driver(form, REQ_DEL_PREV);
			break;

		// Delete the char under the cursor
		case KEY_DC:
			form_driver(form, REQ_DEL_CHAR);
			break;
    
    case '\n':
      form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_PREV_FIELD);
      break;

		default:
      if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
			  form_driver(form, toupper(ch));
      }
			break;
	}

	wrefresh(win_form);
}



void form_hook(formnode * f) {
  FIELD * field = current_field(f);
  int i = field_index(field);  
  char * bufferString=field_buffer(field, 0);
  fprintf(logfile, "Changed field %d addressField=%d Field buffer: %s\n", i, indexToAddress[i].address, field_buffer(field, 0)); 
  int value = strtol(bufferString,NULL,16);
  if (indexToAddress[i].address!=-1) {
    base = (value - indexToAddress[i].address*16) & 0x3ff0;
    updateForm(base);
  }
  if (indexToAddress[i].data!=-1) {
    memory[base+indexToAddress[i].data] = value;
    fprintf(logfile, "base %04X indextToAddress[%d].data=%04X Address %02X Value %02X\n", base, i, indexToAddress[i].data, base+indexToAddress[i].data, value);
  } 
  wrefresh(win_form);
}



int main(int argc, char *argv[]) {
	int ch;
  FIELD ** f;
  int i;
  srand(time(NULL));   // Initialization, should only be called once.
  logfile = fopen("forms.log", "w");
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
  set_escdelay(100);
	win_body = newwin(24, 80, 0, 0);
	assert(win_body != NULL);
	box(win_body, 0, 0);
	win_form = derwin(win_body, 20, 78, 3, 1);
	assert(win_form != NULL);
	box(win_form, 0, 0);
	mvwprintw(win_body, 1, 2, "Press F5 to quit and F2 to print fields content");
 
  for (int i=0;i<sizeof(memory); i++) {
    memory[i] = rand()&0xff; 
  }
  int j=0;
  for (auto line=0; line<16; line++) {
    // Add one address field
    auto tmp = new_field(1,4,line,3,0,0);
    fields.push_back(tmp);
    M m = {-1, line, -1};
    indexToAddress.push_back(m);
    addressFields.push_back(tmp);
    for (auto column=0; column<16; column++) {
      M p = {line*16+column, -1, -1};
      tmp = new_field(1,2,line,8+3*column,0,0);
      indexToAddress.push_back(p);
      fields.push_back(tmp);
      dataFields.push_back(tmp);
    }
    tmp = new_field(1,18,line,57,0,0);
    M q = {-1, -1, line};
    indexToAddress.push_back(q);
    asciiFields.push_back(tmp);
    fields.push_back(tmp);
  }

  for (auto it=addressFields.begin(); it<addressFields.end(); it++) {
    set_field_buffer(*it, 0, "0000");
    set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_STATIC);
    set_field_type(*it, TYPE_REGEXP, "[0-3][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]");
    set_field_just(*it,JUSTIFY_LEFT);
  }

  for (auto it=dataFields.begin(); it<dataFields.end(); it++) {
    set_field_buffer(*it, 0, "0000");
    set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_STATIC);
    set_field_type(*it, TYPE_REGEXP, "[0-9A-Fa-f][0-9A-Fa-f]");
    set_field_just(*it,JUSTIFY_LEFT);
  }

  for (auto it=asciiFields.begin(); it<asciiFields.end(); it++) {
    set_field_buffer(*it, 0, "|................|");
    set_field_opts(*it, O_VISIBLE | O_PUBLIC | O_STATIC);
  }


  f= (FIELD **) malloc((sizeof(FIELD *)) * (addressFields.size()+dataFields.size()+asciiFields.size() + 1) );
  i=0;
  for (auto it=fields.begin(); it<fields.end(); it++) {
    f[i]=*it;
    i++;
  } 
  f[i]=NULL;



	form = new_form(f);
	assert(form != NULL);
  set_field_term(form,form_hook);
	set_form_win(form, win_form);
	set_form_sub(form, derwin(win_form, 18, 76, 1, 1));
	post_form(form);

	refresh();
	wrefresh(win_body);
	wrefresh(win_form);
  form_driver(form, REQ_OVL_MODE);

	while ((ch = getch()) != KEY_F(5))
		driver(ch);

	unpost_form(form);
	free_form(form);
	free_field(fields[0]);
	free_field(fields[1]);
	free_field(fields[2]);
	free_field(fields[3]);
	delwin(win_form);
	delwin(win_body);
	endwin();
  fclose(logfile);
	return 0;
}

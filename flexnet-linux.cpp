// NETPC PROGRAM
// Send and receive data over serial port to look like a disk
// drive to a FLEX machine

#include <stdio.h>
#include <io.h>
#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <alloc.h>
#include <dir.h>
#include <ctype.h>

// constants

#define DLAB 0x80     // send to DATA_FMT register to access baud rate divisor
#define RCV_DATA 0x01 // serial data in - buffer full signal
#define XMIT_RDY 0x20 // serial data out - buffer empty signal
#define TRUE 1
#define FALSE 0
#define ACK 0x06
#define NAK 0x15
#define boolean int

// variables

char filename[20];  // name of .dsk file to be mounted
char curfile[20];
unsigned int checksum;
int tracks;
int sects_trk;
int curtrk;
int cursec;
int sector;
unsigned char mode;
FILE *infile, *confile, *outfile, *tempfile;
unsigned char baud_div;
boolean tmode = TRUE;  //"Talk Mode" or Verbose Mode
char cur_drive;

// these set up for COMM2.  If COMM1 is desired, they
// are changed in the initialization part of the main program.

int RT_REGS = 0x02f8;   // Receive/transmit registers
int INT_ENAB = 0x02f9;  // interrupt enable
int INT_ID = 0x2fa;     // interrupt ID register
int DATA_FMT = 0x2fb;   // data format register -- parity, stop
int MODM_CTRL = 0x2fc;  // modem control -- handshakes
int LINE_STAT = 0x2fd;  // status register for rs-232 line
int MODM_STAT = 0x2fe;  // Modem status register
int BAUD_DIV = 0x2f8;   // set DLAB for access - baud rate divisor


// function prototypes

void init_port(void);          // initialize serial port
char get_stat(void);           // get port status
void mputc(char ch);           // put character to serial
unsigned char mgetc(void);     // get character from serial
void tdelay(long time);        // a time delay loop
void beep(void);               // send BEL character to monitor
void mount(void);              // mount a .dsk file
void rcv_sector(void);         // input a sector
void snd_sector(int sec_num);  // output a sector
void get_sir(void);            // get the system info record
void create(void);             // create a .dsk file
void delete_file(void);        // delete a .dsk file
void send_directory(void);     // directory of .dsk files
void change_dir(void);         // change the current directory
void get_path(void);
void change_drive(void);      // get the current path info.
int find_char(char *mystrng,char ch);

int find_char(char *mystrng,char ch)
{
   int n=0;
   while((mystrng[n] != ch) && (mystrng[n] !=0))
   {
      n++;
   }
   if (mystrng[n] == 0) return 0;
   return n;
}


void change_drive(void)
{
   char command[40];
   char ch;
   int n;
   int pos;
   char cmd[40];

   n = 0;
   ch = mgetc();
   while (ch != 0x0d)
   {
      ch = tolower(ch);
      command[n++] = ch;
      ch = mgetc();
   }
   command[n] = 0; // terminate string

   // find ':' if present
   pos = find_char(command,':');
   // handle case of only drive letter present
   if (pos == 0)
   {
      command[1] = ':';
      command[2] = 0;
      if(tmode) printf("command is %s\n",command);
      system(command);
      cur_drive = command[0];
      mputc(ACK);
      return;
   }
   // case of drive letter and colon
   if ((pos == 1) && (strlen(command) == 2));
   {
      if(tmode) printf("command is %s\n",command);
      system(command);
      cur_drive = command[0];
      mputc(ACK);
   }
   // case of drive letter and path, i.e. d:\path\path2
   if(strlen(command) > 2)
   {
      cmd[0] = command[0];
      cmd[1] = command[1];
      cmd[2] = 0;
      cur_drive = cmd[0];
      if(tmode) printf("drive command is %s\n",cmd);
      system(cmd);

      cmd[0] = 'c';
      cmd[1] = 'd';
      cmd[2] = ' ';
      n=2;
      do
      {
	 cmd[n+1] = command[n];
      } while(command[n++] !=0);
      if(tmode) printf ("path command is %s\n",cmd);
      system(cmd);
      mputc(ACK);
   }
}


void get_path(void)  // get the current path and send to the FLEX machine
{
   char pname[80];
   char fulname[80];
   char ch;
   int n;

   getcurdir(0,pname);
   cur_drive = _getdrive() + 'a' - 1;
   fulname[0] = cur_drive;
   fulname[1] = 0;

   strcat(fulname,":\\");
   strcat(fulname,pname);
   ch = fulname[0];
   n = 1;
   while (ch != 0)
   {
      mputc(ch);
      ch = fulname[n++];
   }
   mputc(0x0d);
   mputc(ACK);
}


void change_dir(void)  // change the directory on PC to the supplied path
{
   int n, result;
   char ch, command[60];
   char before[40];
   char after[40];

   strcpy(command, "cd ");
   n = 3;
   ch = mgetc();
   while(ch != 0x0d)
   {
      command[n++] = ch;
      ch = mgetc();
   }
   command[n] = 0;
   getcurdir(0,before);
   result = system(command);  // system function with path specified
   getcurdir(0,after);
   if((strcmp(before, after) == 0)||(result)) mputc(NAK);
      else mputc(ACK);
}


void send_directory(char cmd) //RDIR, directory of .dsk files in current directory
{
   int n, result;
   char ch1, command[80];
   int ch;
   if(cmd == 'A')
   {
      strcpy(command,"DIR ");
      n = 4;
      ch = mgetc();
      while (ch != 0x0d)
      {
    command[n++] = ch;
    ch = mgetc();
      }
      command[n] = 0x00;
      if(n > 4)
      {
    if(strchr(command,'.')== NULL) strcat(command,".dsk");
      }
      else strcat(command,"*.dsk");
      strcat(command, " > c:\\temp\\temp.txt");
      if(tmode) printf("Dir CMD string : %s\n",command);
   }
   if(cmd == 'I')
   {
      strcpy(command,"DIR /ad /ON > c:\\temp\\temp.txt");
   }

   result = system(command);
   if (result)
   {
      mputc(NAK);
      return;
   }
   tempfile = fopen("c:\\temp\\temp.txt","rb");
 //  ch = fgetc(tempfile);
   ch = 0x0a;
   while (!feof(tempfile))
   {
      mputc(ch);
      putchar(ch); // for debug of missing lines
      if(ch == 0x0a)
      {
    mputc(0x0d);
    ch1 = mgetc(); // wait for a char from FLEX
    if(ch1 == 0x1b)
    {
       if(tmode) printf("dir terminated by ESC\n");
       fclose(tempfile);
       result = system("erase c:\\temp\\temp.txt");
       if(result) mputc(NAK);
       else mputc(ACK);
       return;
    }
      //   else if(tmode) printf("send another line\n");
      }
      ch = fgetc(tempfile);
   }
   if(tmode) printf("End of file reached\n");
   fclose(tempfile);
   result = system("erase c:\\temp\\temp.txt");
   if(result) mputc(NAK);
   else mputc(ACK);
}



void delete_file(void) // RDELETE command, delete named .dsk file in current dir.
{
   int n, result;
   char ch, command[60];
   char name[80];
   FILE *test;

   n = 0;
   ch = mgetc();
   while (ch != 0x0d)
   {
      name[n++] = ch;
      ch = mgetc();
   }
   name[n] = 0x00;
   printf("filename is %s\n",name);
   printf("curfile name is %s\n",curfile);
   if(strcmp(name,curfile) == 0)
   {
      if (tmode) printf("can't delete mounted file\n");
      mputc(NAK);
      return;
   }
   test = fopen(name,"r+b");
   if(test == NULL)
   {
      if(tmode) printf("file doesn't exist or is write protected\n");
      mputc(NAK); // file doesn't exist or is write protected
      return;
   }
   fclose(test); // if it exists we have to close it
   strcpy(command, "erase "); //put the command string together.
   strcat(command,name);
   result = system(command); //don't know why this doesn't catch read only
   if(result) mputc(NAK);  // if not zero an error occurred.
   else mputc(ACK);    // no error
}


void init_port()
{
   outportb(DATA_FMT,0x83);      // access baud rate divide register
   outportb(BAUD_DIV,baud_div);  // 9600 value calc'd from baud rate
   outportb(DATA_FMT,0x03);      // 8 bits 1 stop no parity

   outportb(INT_ENAB,0);         // no interrupts
   outportb(MODM_CTRL,0x02);     // RTS on
   while(get_stat() & RCV_DATA) mgetc(); // clear receive data
}


char get_stat()
{
   return inportb(LINE_STAT);
}


void mputc(char ch)
{
   do
   {
   } while ((get_stat() & XMIT_RDY) == 0);
   outportb(RT_REGS,ch);
}


unsigned char mgetc()
{
   unsigned char ch;
   int n;

   do
   {
      if(mode == 'F')
      {
    if (kbhit()) // terminate on ESC key skip on slow computer
    {
       if (getch() == 0x1b)
       {
          outportb(MODM_CTRL,0x00);
     exit(0);
       }
    }
      } // end if mode == 'F'
   } while ((get_stat() & RCV_DATA) == 0);
   ch = inportb(RT_REGS);
   return ch;
}


void tdelay(long time)
{
   while (time > 0) time = time - 1;
}



/* create a .dsk file in specified path/directory with the
   specified name, number, tracks and sectors   */

void create(void)
{
   int n;
   int sec;
   int trk;
   unsigned int totsec;
   int sectors, tracks;
   int partial;
   char ch;
   char filename[15];
   char diskname[15];
   char disknum[6];
   unsigned int disk_number;
   char numtrks[5];
   char numsecs[5];
   char path[40];
   char command[40];
   struct dosdate_t date; // structure to get date info.

   n = 0;
   ch = mgetc();
   while(ch != 0x0d)
   {
      path[n++] = ch;
      ch = mgetc();
   }
  // if(n!=0) path[n++] = '\\'; // trailing backslash on path
   path[n] = 0;               // terminate string

   n = 0;
   ch = mgetc();
   while(ch != 0x0d)
   {
      filename[n++] = ch;
      ch = mgetc();
   }
   filename[n] = 0; // terminate string
if(tmode) printf("filename is %s\n",filename);

   strcpy(diskname,filename); // name of disk is filename

   n = 0;
   ch = mgetc();
   while(ch != 0x0d)
   {
      disknum[n++] = ch;
      ch = mgetc();
   }
   disknum[n] = 0; // terminate string
   disk_number = atoi(disknum);
if(tmode) printf("disk number is %d\n",disk_number);

   n = 0;
   ch = mgetc();
   while(ch != 0x0d)
   {
      numtrks[n++] = ch;
      ch = mgetc();
   }
   numtrks[n] = 0;
   tracks = atoi(numtrks); // ascii to integer conversion
if(tmode) printf("number of tracks is %d\n",tracks);
   n = 0;
   ch = mgetc();
   while(ch != 0x0d)
   {
      numsecs[n++] = ch;
      ch = mgetc();
   }
   numsecs[n] = 0;
   sectors = atoi(numsecs);
if(tmode) printf("number of sectors per track is %d\n",sectors);

   _dos_getdate(&date);  // get system date for creation date

   totsec = (tracks-1) * sectors;

   if ((path[0] !=0) && (path[0] != cur_drive) &&(path[1] == ':'))
   {
      command[0] = path[0];
      command[1] = ':';
      command[2] = 0;
   if(tmode) printf("Changing drive to %s\n",command);
      system(command);
   }
   strcpy(command,"cd ");
   strcat(command,path);
   if (tmode) printf("Changing path to %s\n",path); //****************
   system(command);  // change directory (or not)
 
   // strcat(path,filename); //path\filename.dsk
   if(strchr(filename,'.')== NULL)   strcat(filename,".dsk");
   if(tmode) printf("filename is %s\n",filename);

   if(tmode) // now have all the parameters, print for verbose
   {
      printf("path %s\n",path);
      printf("filename %s\n",filename);
      printf("disk number %d\n",disk_number);
      printf("tracks %d\n",tracks);
      printf("sectors %d\n",sectors);
      printf("complete name  %s\\%s\n",path,filename);
   }
   outfile = fopen(filename,"wb");  // open for write, binary mode
   if(outfile == NULL)
   {
      if(tmode) printf("can't open output file\n");
      mputc(NAK);
      return;
   }

// write sector 1, all 00 bytes
   for (n=0; n<256; n++)
   {
      fputc(0x00,outfile);
   }

// write sector 2
   fputc(0x00,outfile);
   fputc(0x03,outfile); // sector link

   for (n=0; n<254; n++) // remainder is all 00 bytes
   {
      fputc(0x00,outfile);
   }

// now the hard one, the SIR

   for (n=0; n<16; n++)
   {
      fputc(0x00,outfile); // first 16 bytes all 00
   }
   n = 0;
   ch = diskname[n++];
   while(ch != 0)
   {
      fputc(ch, outfile);
      ch = diskname[n++];
   }
   while (n < 12)
   {
      fputc(0x00,outfile);  // disk name and ext 10 thru 1A
      n++;
   }

   partial = disk_number >> 8;
   fputc(partial,outfile);     // 1B disk number high order
   partial = disk_number & 0x00ff;
   fputc(partial,outfile);     // 1C disk number low order
   fputc(0x01,outfile);        // 1D start of free chain TT 01
   fputc(0x01,outfile);        // 1E start of free chain SS 01
   fputc(tracks-1,outfile);    // 1F End of free chain TT
   fputc(sectors,outfile);     // 20 End of free chain SS
   partial = totsec >> 8;
   fputc(partial,outfile);     // 21 High order num free sectors
   partial = totsec & 0x00ff;
   fputc(partial,outfile);     // 22 Low order num free sectors
   fputc(date.month,outfile);  // 23 Month
   fputc(date.day,outfile);    // 24 Day
   fputc(date.year % 100,outfile);  // 25 Year
   fputc(tracks-1,outfile);    // 26 Last Track
   fputc(sectors,outfile);     // 27 Last Sector

   for (n=40; n<256; n++)      // fill the sector with 00
   {
      fputc(0x00,outfile);
   }

// now the rest of track 0
   for (sec = 4; sec < sectors+1; sec++)
   {
      fputc(0x00,outfile);
      if(sec == sectors) fputc (0x00,outfile);
      else fputc(sec+1,outfile); // link to next sector
      for (n=2; n<256; n++)
      {
    fputc(0x00,outfile);  // 00 bytes except for link
      }
   }

// now do tracks of regular data

   for(trk = 1; trk < tracks; trk++)
   {
      for (sec = 1; sec <= sectors; sec++)
      {
    if (sec == sectors)
    {
       if(trk == tracks-1)
       {
	  fputc(0x00,outfile); // last sector, no link
	  fputc(0x00,outfile);
       }
       else
       {
	  fputc (trk+1,outfile); // link to next trk sect. 01
	  fputc (0x01,outfile);
       }
    }
    else
    {
       fputc(trk,outfile);
       fputc(sec+1,outfile);
    }
    for(n=1; n<255; n++)
    {
       fputc(0x00,outfile);
    }
      }
   }
   fclose (outfile);
   fclose(infile); // close the previously mounted file
   if(tmode) printf("about to mount %s\n",filename);
   infile = fopen(filename,"r+b");
   if(infile == NULL)
   {
      if(tmode) printf("can't mount file\n");
      mputc(NAK);
   }
   else
   {
      get_sir();
      mputc(ACK);
   }
}


void mount(void)  // RMOUNT specifies a .dsk file name to be opened in current dir.
{
   char ch;
   unsigned attrib;
   FILE *test;
   int n=0;
   int k;

   ch = mgetc();
   while(ch != 0x0d)
   {
      filename[n++] = ch;
      ch = mgetc();
   }

   filename[n] = 0; //terminate string
   if(strchr(filename,'.') == NULL) strcat(filename, ".dsk");
   if(tmode) printf("file to be opened: %s\n",filename);
   k=_dos_getfileattr(filename,&attrib);
   if(k !=0)
   {
      if(tmode) printf ("File does not exist\n");
      mputc(NAK);
      return;
   }
   // if (tmode) printf("attribute: %x\n",attrib & FA_RDONLY);

   if(attrib & FA_RDONLY != 0)
   {
       if (tmode) printf("can't open for write\n");
       if(tmode) printf("Trying to open for read only\n");
       test = fopen(filename,"rb");
       if (test == NULL)
       {
	  if(tmode) printf("File does not exist");
	   mputc(NAK);
	   return;
       }
       else
       {
	  fclose(test);
	  fclose(infile); // close previously mounted .dsk file
	  infile =  fopen(filename,"rb");
	  if(tmode) printf("File opened for read only");
	  get_sir();
	  if (tmode) printf("Disk Name: %s\n",filename);
          strcpy(curfile,filename);
	  mputc(ACK);
	  mputc('R'); // open for read only
	  printf("R\n");
       }
       return;
   }
   fclose(infile);                  // close the old one
   infile = fopen(filename,"r+b");  // open for read/update bin.

   get_sir();
   if(tmode) printf("Disk name: %s\n",filename);
   strcpy (curfile, filename);
   mputc(ACK); // acknowledge
   mputc('W'); // open for full access
}


void rcv_sector(void)  // writes a sector to a .dsk file
{
   int check = 0;
   unsigned char ch;
   long filepos;
   unsigned char datas[256];

   checksum = 0;
   for(int n = 0; n < 256; n++) // get data into a buffer
   {
      ch = mgetc();
      datas[n] = ch;
   }
   check = mgetc() <<8;         // get checksum
   check += mgetc();

   if((curtrk == 0) && (cursec == 0))  sector = 0;
   else sector = curtrk * sects_trk  + cursec-1; // calc sector number
   filepos = sector * 256L;     // byte number in file
   fsetpos(infile, &filepos);   // go there

   for(int i = 0; i<256; i++)   // now write the data to the file
   {
      ch = datas[i];
      putc(ch, infile);
      checksum += ch;
   }
   if(tmode) printf("W: %x %x\n",curtrk, cursec);
   if(check != checksum)
   {
      mputc(NAK);
      return;
   }
   mputc(ACK);
}


void get_sir(void)  // get system information record on open .dsk file
{
   int last_trk;

   for(int n = 0; n < 550; n++)  //last trk is byte 550
   {
             // sectors / trk is byte 551
      getc(infile);
   }
   last_trk = getc(infile);
   sects_trk = getc(infile);

   tracks = last_trk + 1;
   if(tmode) printf("Tracks: %d\n",tracks);
   if(tmode) printf("Sectors per track: %d\n",sects_trk);
}


void snd_sector(void)  // read sector from .dsk file and send to FLEX end
{
   int k;
   unsigned char ch;
   long position;

   checksum = 0;

   if((curtrk == 0) && (cursec == 0))  sector = 0;
   else sector = curtrk * sects_trk + cursec-1;

   position = sector * 256L;
   fsetpos(infile, &position);

   for(int n=0; n<256; n++)
   {
      k = getc(infile);
      mputc(k);
      checksum +=k;
   }
   if(tmode) printf("R: %x %x\n",curtrk, cursec);

   mputc((checksum >> 8) & 0x00ff);
   mputc(checksum & 0x00ff);
   mgetc();  // get ack or nak.  Don't need to look at it
}


void main(int argc, char** argv)
{
   int n;
   unsigned char ch;
   int comport;
   long baud_rate;
   char fname[32];
   int sector;

   cur_drive = _getdrive() + 'a' - 1;  // lower case drive number
   confile = fopen("config.txt", "r");
   if(confile !=NULL)
   {
      fscanf(confile,"%d\n",&comport);
      fscanf(confile,"%ld\n",&baud_rate);
      baud_div = 115200/baud_rate;
      fscanf(confile,"%c\n",&mode);
      fscanf(confile,"%c\n",&ch);
      if (ch == 'V') tmode = TRUE;
      else tmode = FALSE;
      fclose(confile);
      if(tmode)
      {
    printf("port selected: %d\n",comport);
    printf("baud rate: %ld\n",baud_rate);
    if(mode == 'S') printf("Slow Computer Mode\n");
    else printf("Fast Computer Mode\n");
      }
   }
   else
   {
      comport = 2; // communications port
      baud_div = 12; // for 9600 baud
      mode = 'S'; // Slow computer < 100 MHz
      tmode = TRUE;
      printf("Can't open config.txt file\n");
      printf("Default baud rate 9600\n");
      printf("Default port, Comm2\n");
      printf("Default mode, Slow Computer\n");
      printf("Default terminal mode, Verbose \n");
   }
   if(tmode) printf("\n\nREMINDER: This program will not run in a DOS window\n");
   if(tmode) printf("It will only run if computer is booted in DOS Mode\n\n");

   if (comport==1)  // comm1 selected
   {
      RT_REGS = 0x03f8;   // Receive/transmit registers
      INT_ENAB = 0x03f9;  // interrupt enable
      INT_ID = 0x3fa;     // interrupt ID register
      DATA_FMT = 0x3fb;   // data format register -- parity, stop
      MODM_CTRL = 0x3fc;  // modem control -- handshakes
      LINE_STAT = 0x3fd;  // status register for rs-232 line
      MODM_STAT = 0x3fe;  // Modem status register
      BAUD_DIV = 0x3f8;   // set DLAB for access - baud rate divisor
      if(tmode) printf("Initializing Comm1\n");
   }
   if((comport == 2) && tmode) printf("Initializing Comm2\n");
   if ((comport == 0) && tmode) printf("Port 0 specified, not initializing port\n");
   else
   {
      init_port();
      if(tmode) printf("Port initialized\n");
   }
// section to open a default file
   strcpy (fname, argv[1]); // default filename from command line
   if(strchr(fname,'.') == NULL) strcat(fname,".dsk");
   infile = fopen(fname,"r+b");
   if(infile == NULL)
   {
      if(tmode) printf("Can't open input file");
      exit(0);
   }

   do
   {
      ch = mgetc();
      mputc(ch); // echo the character
   } while (ch !=0xAA);

   if(tmode) printf("Communication Established\n");

   get_sir();

// command interpreter
   while (TRUE)  // repeat forever loop
   {
      if(kbhit())
      {
    if(getch()==0x1b) exit(0);
      }
      ch = mgetc();

      switch(ch)
      {

// create a .dsk file
    case 'C':
    {
       create();
       break;
    }

// mount a new .dsk file as drive 3
    case 'M':
    {
       mount();
       break;
    }

// receive a sector from the FLEX system
    case 'R':
    {
       curtrk = mgetc();
       cursec = mgetc();
       rcv_sector();
       break;
    }

// send a sector to the FLEX system
    case 'S':
    {
       curtrk = mgetc();
       cursec = mgetc();
       snd_sector();
       break;
    }

// change directory
    case 'P':  // for point
    {
       change_dir();
       break;
    }

// disk directory of current or sent path
    case 'A':  // for cAt
    {
       send_directory('A');
       break;
    }

// List of subdirectories in current directory
    case 'I':  // for dIrlist
    {
       send_directory('I');
       break;
    }


// delete a file in current directory or sent path
    case 'D':
    {
       delete_file();
       break;
    }

// quit program
    case 'Q':
    {
       mputc(ACK);
       break;
    }

 // return current path and directory
    case '?':
    {
       get_path();
       break;
    }
    case 'V':
    {
       change_drive();
       break;
    }

    case 0x55:
    {
       mputc(0x55);
       break;  // to ignore RESYNC if sent
    }
    case 0xaa:
    {
       mputc(0xaa);
       break;  // while program is running
    }
    case 'E':
    {
       mputc(ACK);
       exit(0);
    }
    default:
    {
       if(tmode) printf("%c %x  ",ch,ch);
       if(tmode) printf("Illegal Command!\n");
    }
  //       better just go around again for another command.
      }  // end switch
   } // end while true
}

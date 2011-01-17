#ifndef IOPOLL_H
#define IOPOLL_H

#define EV_READ 0

int iopoll_create(); 
void iopoll_event(int s, int ev);
int iopoll_wait();
void iopoll_readable(int s);
void iopoll_writable(int s);


#endif

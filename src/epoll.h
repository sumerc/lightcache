#ifndef IOPOLL_H
#define IOPOLL_H

int iopoll_create(); 
iopoll_event(int s, int ev);
iopoll_wait();
iopoll_readable(int s);
iopoll_writable(int s);

#endif
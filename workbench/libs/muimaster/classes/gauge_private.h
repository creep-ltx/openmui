#ifndef _GAUGE_PRIVATE_H_
#define _GAUGE_PRIVATE_H_

#define GAUGE_BUFSIZE 256

/*** Instance data **********************************************************/
struct Gauge_DATA
{
   BOOL horiz;
   BOOL dupinfo;

   ULONG current; /* LONG in MUI, but MUI seems to handle it like ULONG */
   ULONG max;
   ULONG divide;
   STRPTR info;

   LONG info_width;
   LONG info_height;
   char *buf;     /* the info text formatted, GAUGE_BUFSIZE bytes: made
                   * when there is an info text (Gauge_Format) */
};


#endif /* _GAUGE_PRIVATE_H_ */

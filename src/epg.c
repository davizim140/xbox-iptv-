#include "epg.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *find_tag_end(const char *p) { return strchr(p, '>'); }
static void decode_text(char *out, size_t cap, const char *a, const char *b)
{
 size_t n=0;
 while(a<b && n+1<cap) {
  if(*a=='&') {
   const char *semi=memchr(a,';', (size_t)(b-a));
   if(semi) {
    size_t len=(size_t)(semi-a+1); char ch=0;
    if(len==5 && memcmp(a,"&amp;",5)==0) ch='&';
    else if(len==4 && memcmp(a,"&lt;",4)==0) ch='<';
    else if(len==4 && memcmp(a,"&gt;",4)==0) ch='>';
    else if(len==6 && memcmp(a,"&quot;",6)==0) ch='"';
    else if(len==6 && memcmp(a,"&apos;",6)==0) ch='\'';
    if(ch) { out[n++]=ch; a=semi+1; continue; }
   }
  }
  if(*a!='\r' && *a!='\n') out[n++]=*a;
  a++;
 }
 out[n]=0;
}
static bool attr_value(const char *tag, const char *name, char *out, size_t cap)
{
 char key[40];
 if(snprintf(key,sizeof(key),"%s=\"",name)>=(int)sizeof(key)) return false;
 const char *p=strstr(tag,key); if(!p) return false; p+=strlen(key);
 const char *e=strchr(p,'"'); if(!e) return false;
 size_t n=(size_t)(e-p); if(n>=cap) return false;
 memcpy(out,p,n); out[n]=0; return true;
}
static bool element_text(const char *begin,const char *end,const char *tag,char *out,size_t cap)
{
 char open[32],close[32];
 (void)snprintf(open,sizeof(open),"<%s",tag);
 (void)snprintf(close,sizeof(close),"</%s>",tag);
 const char *p=strstr(begin,open); if(!p || p>=end) return false;
 const char *gt=find_tag_end(p); if(!gt || gt>=end) return false;
 const char *finish=strstr(gt+1,close); if(!finish || finish>end) return false;
 decode_text(out,cap,gt+1,finish); return true;
}
int iptv_epg_parse(const char *xml,IptvProgramme *items,size_t capacity)
{
 if(!xml || (capacity && !items)) return -1;
 size_t count=0; const char *p=xml;
 while((p=strstr(p,"<programme"))!=NULL) {
  const char *gt=find_tag_end(p); if(!gt) break;
  const char *end=strstr(gt+1,"</programme>"); if(!end) break;
  if(count<capacity) {
   IptvProgramme item; memset(&item,0,sizeof(item));
   (void)attr_value(p,"channel",item.channel_id,sizeof(item.channel_id));
   (void)attr_value(p,"start",item.start,sizeof(item.start));
   (void)attr_value(p,"stop",item.stop,sizeof(item.stop));
   (void)element_text(gt+1,end,"title",item.title,sizeof(item.title));
   (void)element_text(gt+1,end,"desc",item.description,sizeof(item.description));
   if(item.channel_id[0] && item.title[0]) items[count++]=item;
  }
  p=end+12;
 }
 return (int)count;
}

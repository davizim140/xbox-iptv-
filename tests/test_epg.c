#include "epg.h"
#include <assert.h>
#include <string.h>
int main(void) {
 const char *xml="<?xml version=\"1.0\"?><tv><programme start=\"20261120190000 -0300\" stop=\"20261120200000 -0300\" channel=\"br.news\"><title>Jornal &amp; Noticias</title><desc>Resumo &lt;ao vivo&gt;</desc></programme></tv>";
 IptvProgramme p[2]; int n=iptv_epg_parse(xml,p,2);
 assert(n==1); assert(strcmp(p[0].channel_id,"br.news")==0);
 assert(strcmp(p[0].title,"Jornal & Noticias")==0);
 assert(strcmp(p[0].description,"Resumo <ao vivo>")==0);
 assert(strcmp(p[0].start,"20261120190000 -0300")==0);
 assert(iptv_epg_parse("<tv></tv>",p,2)==0);
 return 0;
}
#include <stdio.h>
#include <stdlib.h>

#include "opt.h"
#include "util.h"
#include "pull/pull.h"
static int in_resume=0;
static char *flags =NULL;
struct install_impls *install_impl;

struct install_impls *impls_to_install[]={
  &impls_sbcl_bin,
  &impls_sbcl
};

int installed_p(char* impl,char* version) 
{
  int ret;
  char* i;
  int pos;
  pos=position_char("-",impl);
  if(pos!=-1) {
    impl=subseq(impl,0,pos);
  }else
    impl=q(impl);
  i=s_cat(homedir(),q("impls/"),q(impl),q("-"),q(version),q("/"),NULL);
  ret=directory_exist_p(i);
  s(i),s(impl);
  return ret;
}

int install_running_p(char* impl,char* version)
{
  /* TBD */
  return 0;
}

int start(char* impl,char* version)
{
  char* home= homedir();
  char* p;
  ensure_directories_exist(home);
  if(installed_p(impl,version)) {
    printf("%s/%s are already installed.if you intend to reinstall by (TBD).\n",impl,version);
    return 0;
  }
  if(install_running_p(impl,version)) {
    printf("It seems running installation process for $1/$2.\n");
    return 0;
  }
  p=cat(home,"tmp/",impl,"-",version,"/",NULL);
  ensure_directories_exist(p);
  s(p);

  p=cat(home,"tmp/",impl,"-",version,".lock",NULL);
  setup_signal_handler(p);
  touch(p);

  s(p);
  s(home);
  return 1;
}

int download(char* impl,char* version)
{
  char* home= homedir();
  char* url;
  char* impl_archive;
  url=(*(install_impl->uri))(impl,version);
  if(get_opt("skip.download")) {
    printf("Skip downloading %s\n",url);
  }else {
    printf("Downloading archive.:%s\n",url);
    /*TBD proxy support... etc*/
    if(url) {
      impl_archive=cat(home,"archives/",impl,"-",version,".",(*(install_impl->extention))(impl,version),NULL);
      ensure_directories_exist(impl_archive);

      if(download_simple(url,impl_archive,0)) {
	printf("Failed to Download.\n");
	return 0;
	/* fail */
      }else{
	printf("download done:%s\n",url);
      }
    }
    s(impl_archive),s(url);
  }
  s(home);
  return 1;
}

int expand(char* impl,char* version)
{
  char* home= homedir();
  char* argv[5]={"-xf",NULL,"-C",NULL,NULL};
  int argc=4;
  int pos;
  char* archive;
  char* dist_path;
  archive=cat(impl,"-",version,".",(*(install_impl->extention))(impl,version),NULL);
  pos=position_char("-",impl);
  if(pos!=-1) {
    impl=subseq(impl,0,pos);
  }else
    impl=q(impl);
  version=q(version);
  dist_path=cat(home,"src/",impl,"-",version,"/",NULL);

  printf("Extracting archive. %s to %s\n",archive,dist_path);
  
  delete_directory(dist_path,1);
  ensure_directories_exist(dist_path);

  /* TBD log output */
  argv[1]=cat(home,"archives/",archive,NULL);
  argv[3]=cat(home,"src/",NULL);
  cmd_tar(argc,argv);

  s(argv[1]),s(argv[3]);
  s(impl);
  s(dist_path);
  s(archive);
  s(home);
  s(version);
  return 1;
}

int configure(char* impl,char* version)
{
  char* home= homedir();
  char* confgcache= cat(home,"/src/",impl,"-",version,"/src/confg.cache",NULL);
  char* cd;
  char* configure;
  int ret;
  if(in_resume) {
    delete_file(confgcache);
  }
  if(flags==NULL) {
    flags=q("");
  }
  if(strcmp("gcl",impl)==0) {
    flags=s_cat(flags,q(" --enable-ansi"));
  }
  if(strcmp("ecl",impl)==0 ||strcmp("ecl",impl)==0 || strcmp("clisp",impl)==0) {
    flags=s_cat(flags,q(" --mandir="),q(home),q("/share/man"));
  }
  printf("Configuring %s/%s\n",impl,version);
  cd=cat(home,"src/",impl,"-",version,NULL);
  printf ("cd:%s\n",cd);
  change_directory(cd);
  /* pipe connect for logging cim_with_output_control */
  configure=cat("./configure ",flags," --prefix=",home,"/impls/",impl,"-",version,NULL);
  ret=system(configure);
  s(configure);
  s(cd);
  s(confgcache);
  s(home);
}

install_cmds install_full[]={
  start,
  download,
  expand,
  configure,
  NULL
};

int cmd_pull(int argc,char **argv)
{
  int ret=1,k;
  install_cmds *cmds=NULL;
  if(argc!=0) {
    for(k=0;k<argc;++k) {
      char* impl=argv[k];
      char* version_arg=NULL;
      char* version=NULL;
      int i,pos;
      pos=position_char("/",impl);
      if(pos!=-1) {
	version_arg=subseq(impl,pos+1,0);
	impl=subseq(impl,0,pos);
      }else {
	impl=q(impl);
      }

      for(install_impl=NULL,i=0;i<sizeof(impls_to_install)/sizeof(struct install_impls*);++i) {
	struct install_impls* j=impls_to_install[i];
	if(strcmp(impl,j->name)==0) {
	  install_impl=j;
	}
      }
      if(!install_impl) {
	printf("%s is not implemented for install.\n",impl);
	exit(EXIT_FAILURE);
      }
      version=(*(install_impl->version))(impl,version_arg);
      for(cmds=install_impl->call;*cmds&&ret;++cmds) {
	ret=(*cmds)(impl,version);
      }
      { // after install latest installed impl/version should be default for 'run'
        struct opts* opt=global_opt;
        struct opts** opts=&opt;
        int i;
        char* home=homedir();
        char* path=cat(home,"config",NULL);
        char* v=cat(impl,".version",NULL);
        for(i=0;version[i]!='\0';++i)
          if(version[i]=='-')
            version[i]='\0';
        set_opt(opts,"default.impl",impl,0);
        set_opt(opts,v,version,0);
        save_opts(path,opt);
        s(home),s(path),s(v);
      }
      s(version),s(version_arg),s(impl);
    }
  }else {
    printf("what would you like to install?\n");
    exit(EXIT_FAILURE);
  }
  return ret;
}
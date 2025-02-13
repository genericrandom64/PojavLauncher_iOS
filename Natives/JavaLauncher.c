#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "jni.h"
#include "log.h"
#include "JavaLauncher.h"

// PojavLancher: fixme: are these wrong?
#define FULL_VERSION "1.16.0-internal"
#define DOT_VERSION "1.16"

static const char* const_progname = "java";
static const char* const_launcher = "openjdk";
static const char** const_jargs = NULL;
static const char** const_appclasspath = NULL;
static const jboolean const_javaw = JNI_FALSE;
static const jboolean const_cpwildcard = JNI_TRUE;
static const jint const_ergo_class = 0; // DEFAULT_POLICY

typedef jint JLI_Launch_func(int argc, char ** argv, /* main argc, argc */
        int jargc, const char** jargv,          /* java args */
        int appclassc, const char** appclassv,  /* app classpath */
        const char* fullversion,                /* full version defined */
        const char* dotversion,                 /* dot version defined */
        const char* pname,                      /* program name */
        const char* lname,                      /* launcher name */
        jboolean javaargs,                      /* JAVA_ARGS */
        jboolean cpwildcard,                    /* classpath wildcard*/
        jboolean javaw,                         /* windows-only javaw */
        jint ergo                               /* ergonomics class policy */
);

int launchJVM(int argc, char *argv[]) {
    debug("Beginning JVM launch\n");
    setenv("LIBGL_FB", "2", 1);
    setenv("LIBGL_MIPMAP", "3", 1);
    setenv("LIBGL_NORMALIZE", "1", 1);
    
    char *java_libs_dir = "/Applications/PojavLauncher.app/libs";
    
    char *args_path = "/var/mobile/Documents/minecraft/overrideargs.txt";
    char *log_path = "/var/mobile/Documents/minecraft/latestlog.txt";
    char classpath[10000];
    
    // "/Applications/PojavLauncher.app/libs/launcher.jar:/Applications/PojavLauncher.app/libs/ExagearApacheCommons.jar:/Applications/PojavLauncher.app/libs/gson-2.8.6.jar:/Applications/PojavLauncher.app/libs/jsr305.jar:/Applications/PojavLauncher.app/libs/lwjgl3-minecraft.jar";
    
    // Generate classpath
    DIR *d;
    struct dirent *dir;
    d = opendir(java_libs_dir);
    int cplen = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            cplen += sprintf(classpath + cplen, "%s/%s:", java_libs_dir, dir->d_name);
        }
        closedir(d);
    }
    debug("Classpath generated: %s", classpath);
    
    debug("Staring logging STDIO as jrelog:V\n");
    // Redirect stdio to latestlog.txt
    FILE* logFile = fopen(log_path, "w");
    int log_fd = fileno(logFile);
    dup2(log_fd, 1);
    dup2(log_fd, 2);
    close(log_fd);
    
    int margc = 0;
    char* margv[1000];
    // Check if JVM restarts
    if (!started) {
        margv[margc++] = "/usr/lib/jvm/java-16-openjdk/bin/java";
        margv[margc++] = "-XstartOnFirstThread";
        margv[margc++] = "-Djava.library.path=/Applications/PojavLauncher.app/Frameworks";
        margv[margc++] = "-Dorg.lwjgl.opengl.libname=libGL.dylib";
        char jvmargs[10000];
        FILE* argsFile = fopen(args_path, "r");
        debug("Reading custom JVM args (overrideargs.txt), opened=%d\n", argsFile != NULL);
        if (argsFile != NULL) {
            fscanf(argsFile, "%s", jvmargs);
            char *pch;
            pch = strtok(jvmargs, " ");
            while (pch != NULL) {
                debug("Added custom arg: %s\n", pch);
                margv[margc++] = pch;
                pch = strtok(NULL, " ");
            }
            fclose(argsFile);
        }
        margv[margc++] = "-cp";
        margv[margc++] = classpath;
        margv[margc++] = "net.kdt.pojavlaunch.PLaunchApp";
        
        for (int i = 0; i < argc; i++) {
            margv[margc++] = argv[i];
        }
    }
    
    // Load java
    void* libjli = dlopen("/usr/lib/jvm/java-16-openjdk/lib/libjli.dylib", RTLD_LAZY | RTLD_GLOBAL);

    if (NULL == libjli) {
        debug("JLI lib = NULL: %s\n", dlerror());
        return -1;
    }
    debug("Found JLI lib\n");

    JLI_Launch_func *pJLI_Launch =
          (JLI_Launch_func *)dlsym(libjli, "JLI_Launch");
          
    if (NULL == pJLI_Launch) {
        debug("JLI_Launch = NULL\n");
        return -2;
    }

    debug("Calling JLI_Launch\n");
    
    int targc = started ? argc : margc;
    char **targv = started ? argv : margv;
    
    if (!started) {
        started = true;
    }

    return pJLI_Launch(targc, targv,
                   0, NULL, // sizeof(const_jargs) / sizeof(char *), const_jargs,
                   0, NULL, // sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   const_progname, // (const_progname != NULL) ? const_progname : *margv,
                   const_launcher, // (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}

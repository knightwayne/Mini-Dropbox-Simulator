#include <stdio.h>
#include "filedata.h"

void test_add_client(struct login_message lm) {
    if(add_client(lm)){
        printf("client %s is in %s\n", lm.userid, lm.dir);
    } else {
        printf("client %s added to %s\n", lm.userid, lm.dir);
    }
}

void test_checkfile(struct file_info *f, char *file){
    struct file_info *newf;
    if((newf = check_file(f, file)) != NULL){
        printf("found %s\n", file);
    } else if(newf == NULL){
        printf("not found -  %s\n", file);
    }
    
}

int main() {
    
    init();
    struct login_message lm = {"karen", "dir1"};
    
    test_add_client(lm);
    struct login_message lm1 = {"paul", "dir2"};
    
    test_add_client(lm1);
    struct login_message lm2= {"jen", "dir1"};
    test_add_client(lm2);
    
    
    test_checkfile(clients[0].files, "file1");
    test_checkfile(clients[0].files, "file1");
    test_checkfile(clients[1].files, "file2");
    test_checkfile(clients[2].files, "file1");
    
    display_clients();
    
    
    return 0;
}
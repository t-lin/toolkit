#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    // TODO: Make this not have to input in plain text in terminal...
    if (argc != 3) {
        printf("ERROR: Incorrect number of parameters.\n");
        printf("USAGE: ldapasswd-savi <oldpassword> <newpassword>\n");
        exit(EXIT_FAILURE);
    }

    int ret = 0;
    char *user = getenv("USER");
    char *oldpass = argv[1];
    char *newpass = argv[2];

    if (user == NULL) {
        printf("ERROR: Unable to get username\n");
        exit(EXIT_FAILURE);
    }

    // Store IP and password as binary as simple obfuscation
    // TODO: Need better methods to protect against advanced efforts to read binary
    //       Something involving ROT13?
    har ldapIP[4] = {10, 20, 30, 254}; // Set LDAP IP here
    char ldapPass[9] = {'Y', 'o', 'u', 'r', 'P', 'a', 's', 's', '\0'}; // Set LDAP password here

    //char *commandToken = "ldappasswd -H ldap://%hhu.%hhu.%hhu.%hhu -x -D \"cn=admin,dc=savitestbed,dc=ca\" -w %s -A -S \"uid=%s,ou=People,dc=savitestbed,dc=ca\"";
    char *commandToken = "ldappasswd -H ldap://%hhu.%hhu.%hhu.%hhu -x -D \"cn=admin,dc=savitestbed,dc=ca\" -w %s -a %s -s %s \"uid=%s,ou=People,dc=savitestbed,dc=ca\"";
    char completedCommand[250] = {0};
    sprintf(completedCommand, commandToken, ldapIP[0], ldapIP[1], ldapIP[2], ldapIP[3], ldapPass, oldpass, newpass, user);
    //printf("Command: %s\n", completedCommand);

    //printf ("Changing password...\n");
    ret = system (completedCommand);
    //printf ("The value returned was: %d.\n", ret);

    if (ret != 0) {
        printf("ERROR: Password change returned non-zero status, may not have gone through");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

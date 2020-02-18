#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

int execTokens(char** tokens, int numTokens, char wait, char* outFile);

/*
 * Returns:
 * 1 if char is a bash operator
 * 0 if not
 */
char isOperator(char c) {
    return c == '&' || c == '|' || c == '<'
        || c == '>' || c ==';' || c == '('
        || c == ')';
}

/*
 * Given an index and empty token, parses a string for the next bash token,
 *  then:
 * Returns index directly after token
 */
int get_next_token(char* line, char* token, int ii) {
    //if space or tab, recur starting at next char
    if (line[ii] == ' ' || line[ii] == '\t') {
        return get_next_token(line, token, ii+1);
        //if operator
    } else if (isOperator(line[ii])) {
        token[0] = line[ii];

        //if | or &, check if it is actually && or ||
        if ((line[ii] == '&' || line[ii] == '|') && line[ii+1] == line[ii]) {
            token[1] = line[ii];
            token[2] = '\0';
            ++ii;
        } else {
            token[1] = '\0';
        }
        ++ii;

        //if any other text
    } else {
        int jj = 0;
        char quotes = 0;
        while ((line[ii] != '\n' && line[ii] != ' '
                    && !isOperator(line[ii]) && line[ii] != '\t')
                || quotes) {
            if (line[ii] == '"') quotes = !quotes;
            else {
                token[jj] = line[ii];
                ++jj;
            }
            ++ii;
        }
        token[jj] = '\0';
    }
    return ii;
}

/*
 * Given a command string and a 2D array, populates the 2D array with
 *  parsed tokens from command.
 * Returns number of tokens found
 */
int tokenize(char* line, char** tokens) {
    int ii = 0;
    int jj = 0;

    while (line[ii] != '\n') {
        char* token = malloc(256);
        ii = get_next_token(line, token, ii);

        if (token[0] != '\0') {
            tokens[jj] = token;
            jj++;
        } else {
            free(token);
        }
    }

    return jj;
}

void execPipe(char** lhs, int numLhs, char** rhs, int numRhs) {
    int cpid;

    int pipes[2];
    pipe(pipes);

    // pipes[0] is for reading
    // pipes[1] is for writing

    if ((cpid = fork())) {
        // parent

        close(pipes[1]);

        int status;
        waitpid(cpid, &status, 0);
    }
    else {
        // child
        close(pipes[0]);
       
        dup2(pipes[1], 1);

        exit(execTokens(lhs, numLhs, 1, NULL));
    }

    int cpid2;

    if ((cpid2 = fork())) {
        close(pipes[0]);

        int status;
        waitpid(cpid2, &status, 0);
    } else {
        dup2(pipes[0], 0);

        exit(execTokens(rhs, numRhs, 1, NULL));
    }

}

char findOperators(char** tokens, int numTokens) {
    int startIndex = 0;
    int nOpen = 0;
    if (tokens[0][0] == '(') {
        nOpen = 1;
        for (int ii = 1; ii < numTokens; ++ii) {
            if (!strcmp(tokens[ii], "(")) {
                ++nOpen;
            } else if (!strcmp(tokens[ii], ")")) {
                --nOpen;
                if (nOpen == 0) {
                    if (ii == numTokens-1) {
                        execTokens(tokens+1, numTokens-2, 1, NULL);
                        return 1;
                    } else {
                        startIndex = ii + 1;
                    }
                    break;
                }
            }
        }
    }

    nOpen = 0;
    for (int ii = startIndex; ii < numTokens; ++ii) {
        if (!strcmp(tokens[ii], "(")) {
            ++nOpen;
        } else if (!strcmp(tokens[ii], ")")) {
            --nOpen;
        } else if (!strcmp(tokens[ii], ";") && nOpen == 0) {
            char** lhs = malloc(256);
            char** rhs = malloc(256);
            for (int jj = 0; jj < ii; ++jj) {
                lhs[jj] = tokens[jj];
            }
            for (int jj = 0; jj < numTokens-ii-1; ++jj) {
                rhs[jj] = tokens[ii+jj+1];
            }
            execTokens(lhs, ii, 1, NULL);
            execTokens(rhs, numTokens-ii-1, 1, NULL);
            free(lhs);
            free(rhs);
            return 1;
        }
    }

    for (int ii = startIndex; ii < numTokens; ++ii) {
        if (tokens[ii][0] == '|' || tokens[ii][0] == '&' || tokens[ii][0] == '>' 
                || tokens[ii][0] == '<') {
            char** lhs = malloc(256);
            char** rhs = malloc(256);
            for (int jj = 0; jj < ii; ++jj) {
                lhs[jj] = tokens[jj];
            }
            for (int jj = 0; jj < numTokens-ii-1; ++jj) {
                rhs[jj] = tokens[ii+jj+1];
            }

            if (!strcmp(tokens[ii], "<") && numTokens-ii-1 > 0) {
                FILE* in = freopen(rhs[0], "r", stdin);
                execTokens(lhs, ii, 1, NULL);
                fclose(in);
                freopen("/dev/stdin", "r", stdin);
            } else if (!strcmp(tokens[ii], ">") && numTokens-ii-1 > 0) {
                    execTokens(lhs, ii, 1, rhs[0]);
            } else if (!strcmp(tokens[ii], "|")) {
                execPipe(lhs, ii, rhs, numTokens-ii-1);
            } else if (!strcmp(tokens[ii], "||")) {
                if (execTokens(lhs, ii, 1, NULL)) {
                    execTokens(rhs, numTokens-ii-1, 1, NULL);
                }
            } else if (!strcmp(tokens[ii], "&")) {
                execTokens(lhs, ii, 0, NULL);
            } else if (!strcmp(tokens[ii], "&&")) {
                if (!execTokens(lhs, ii, 1, NULL)) {
                    execTokens(rhs, numTokens-ii-1, 1, NULL);
                }
            }
            free(lhs);
            free(rhs);
            return 1;
        }
    }
    return 0; 
}


int execTokens(char** tokens, int numTokens, char wait, char* outFile) {

    if (numTokens == 0) {
        return 0;
    }


    if (findOperators(tokens, numTokens)) {
        return 0;
    }

    if (!strcmp(tokens[0], "cd")) {
        if (numTokens >= 2) {
            chdir(tokens[1]);
            return 0;
        } else {
            puts("Insufficient number of arguments");
            return 1;
        }
    }

    if (!strcmp(tokens[0], "exit")) {
        exit(0);
    }

    int cpid;

    if ((cpid = fork())) {
        // parent process
        // Child may still be running until we wait.

        if (wait) {
            int status;
            waitpid(cpid, &status, 0);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
        } else {
            return 0;
        }
    }
    else {
        if (outFile != NULL) {
            freopen(outFile, "w", stdout);
            fflush(stdout);
        }
        tokens[numTokens] = NULL;
        execvp(tokens[0], tokens);
        //printf("Can't get here, exec only returns on error.\n");
        return 1;

    }
    return 0;
}

void execute(char* cmd)
{
    char** tokens = malloc(256);
    int numTokens = tokenize(cmd, tokens);
    execTokens(tokens, numTokens, 1, NULL);
    for (int ii = 0; ii < numTokens; ++ii) {
        free(tokens[ii]);
    }
    free(tokens);
}

int main(int argc, char* argv[])
{
    char cmd[256];

    if (argc == 1) {
        while(1) {
            printf("nush$ ");
            fflush(stdout);
            if (fgets(cmd, 256, stdin) == NULL) {
                puts("");
                exit(0);
            }
            while(cmd[strlen(cmd)-2] == '\\') {
                fgets(cmd+strlen(cmd)-2, 256, stdin);
            }
            execute(cmd);
        }
    }
    else {
        FILE* fd = fopen(argv[1], "r");
        if (fd == NULL) {
            puts("Could not open file");
            exit(1);
        }
        while(fgets(cmd, 256, fd)) {

            while (cmd[strlen(cmd)-2] == '\\') {
                fgets(cmd+strlen(cmd)-2, 256, fd);
            }
            execute(cmd);
        }
    }
    return 0;
}

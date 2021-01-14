#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

////////////////////////////////////////////////////
// CSCI 503 Operating System
// Lab 2 ¡V A Simple Shell 
// Part 2: Supporting Execution of Shell Commands
// Author: Chu-An Tsai
////////////////////////////////////////////////////

// This Lab2 is part 2 for A Simple Shell, so I canceled the syntax output and rearranged some validation and parser functions. 
// One new function added is the actual_execution for execute the real commands.

// A function to actually execute the commands
int actual_execution(char*** commands, int number_subcommands, int is_background, int is_output, int is_output_append, char* output_file)
{
	if(!commands)
	{
		return -1;
	}
	// Execvp return value;
	int execvp_ret;
	// Return value of fork()
	pid_t ret;
	// This is the read end for previous iteration
	int read_end = 0;
	// Pipe return value
	int pipe_ret;
	// Pipe file descriptor array
	int pipe_file_descriptor[2];
	// Child process return value
	int* child_process = (int*)malloc(sizeof(int)*number_subcommands);	
	int i = 0;
	for(i = 0; i < number_subcommands; i++)
	{
		// Pipe creation
		pipe_ret = pipe(pipe_file_descriptor);
		if(pipe_ret < 0)
		{
			perror("pipe creation fail");
			return -1;
		}
		// Fork a child process
		ret = fork();
		if(ret < 0)
		{
			perror("child process fork fail");
			return -1;
		}
		else if(ret == 0)
		{
			// The child process
			if(read_end != 0 || number_subcommands == 1)
			{
				if(i == number_subcommands - 1)
				{
					if(dup2(read_end, 0) < 0)
					{
						perror("dup2");
						exit(1);
					}
					// Output redirection
					if(is_output && output_file)
					{
						int output_file_descriptor = open(output_file, O_CREAT|O_TRUNC|O_WRONLY, 0644);
						if(output_file_descriptor < 0)
						{
							perror("file open fail");
							exit(1);
						}
						dup2(output_file_descriptor, 1);
					}
					// Output append redirection
					if(is_output_append && output_file)
					{
						int output_file_descriptor = open(output_file, O_CREAT|O_APPEND|O_WRONLY, 0644);
						if(output_file_descriptor < 0)
						{
							perror("file open fail");
							exit(1);
						}
						dup2(output_file_descriptor, 1);
					}
					// Background running
					if(is_background)
					{
						// Set the child process to belong to another group.
						setpgid(0, 0);
					}
					execvp_ret = execvp(commands[i][0], (char * const *)commands[i]);
					// Execution check
					if(execvp_ret < 0)
					{
                        perror("execvp fail");
	                    return -1;
		            }
					break;
				}
				else
				{
					// Copy stdin to read end file descriptor
					if(dup2(read_end, 0)<0)
					{
						perror("dup2");
                    	exit(1);
					}
					close(read_end);
				}
			}
			if(pipe_file_descriptor[1] != 1)
			{
				// Point to the write end
				if(dup2(pipe_file_descriptor[1], 1) < 0)
				{
					perror("dup2");
					exit(1);
				}
				close(pipe_file_descriptor[1]);
			}
			execvp_ret = execvp(commands[i][0], (char * const *)commands[i]);
			if(execvp_ret < 0)
			{
				perror("execvp fail");
				return -1;
			}
		}
		else
		{
			// Parent process only need the read end for the next iteration
			close(pipe_file_descriptor[1]);
			read_end = pipe_file_descriptor[0];
		}
	}
	close(pipe_file_descriptor[1]);
	close(pipe_file_descriptor[0]);
	// No child process should be waited if the commmand line requires to run read_end background
	// But it is better to wait for child process.
	if(!is_background)
	{
		int n = 0;
		for(n = 0; n < number_subcommands; n++)
		{
			wait(&(child_process[n]));
		}
	}
	// Free space
	free(child_process);
 	return 0; 
}

// A function to parse tokens
char** parse_subcommand_tokens(char* one_subcommand)
{
	int command_space = 2;
	char** subcommand_token_array = (char**) malloc(sizeof(char*) * command_space);
	// Allocation check
	if(!subcommand_token_array)
	{
		perror("subcommand_token_array allocated fail");
		return NULL;
	}
	// Parse the subcommand_tokens string with space
	char* cut_tokens = strtok(one_subcommand, " <\n\t\r");
	int count_number_tokens = 0;
	// Check typo or error '
	int count_only_one_quote = 0;
	while(cut_tokens)
	{
		if(strchr(cut_tokens,'\'') || count_only_one_quote == 1)
		{
			if(count_only_one_quote == 0 && strchr(cut_tokens, '\''))
			{
				int number_characters = sizeof(cut_tokens) / sizeof(cut_tokens[0]);
				char* cut_sub_subtoken = (char *)malloc(sizeof(char)*(number_characters + 1));
				int j = 0;
				int k = 0;
				for(k=0;k<number_characters; k++)
				{
					if(cut_tokens[k] == '\''){
						continue;
					}
					else
					{
						cut_sub_subtoken[j] = cut_tokens[k];
						j++;
					}
				}
				strcat(cut_sub_subtoken, " ");
				subcommand_token_array[count_number_tokens] = cut_sub_subtoken;
				count_only_one_quote = 1;
			}
			else if(count_only_one_quote == 1 && !strchr(cut_tokens,'\''))
			{
				strcat(subcommand_token_array[count_number_tokens], cut_tokens);
				strcat(subcommand_token_array[count_number_tokens], " ");
			}
			else if(strchr(cut_tokens, '\'') && count_only_one_quote == 1)
			{
				char* cut_sub_sub_subtoken = strtok(cut_tokens, "\'");
				while(cut_sub_sub_subtoken)
				{
					strcat(subcommand_token_array[count_number_tokens], cut_tokens);
					cut_sub_sub_subtoken = strtok(NULL, "\'");
				}
				count_number_tokens ++;
				count_only_one_quote = 0;
			}
		}
		else
		{
			subcommand_token_array[count_number_tokens] = cut_tokens;
			count_number_tokens ++;
		}
		// Extend space if the buffer is full
		if(count_number_tokens + 1 >= command_space)
		{
			command_space = command_space + 2;
			subcommand_token_array = (char **) realloc( subcommand_token_array, sizeof(char*)*command_space);
			if(!subcommand_token_array)
			{
				perror("subcommand_token_array reallocation fail");
				return NULL;
			}
		}
		cut_tokens = strtok(NULL, " <\n\t\r");
	}
	// If the command is empty
	if(count_number_tokens == 0)
	{
		return NULL;
	}
	// Use NULL to end the command line
	subcommand_token_array[count_number_tokens] = NULL;

	return subcommand_token_array;
}

// A function to parse user input
char*** parse_input(char* input_line, size_t number_chars, int* number_subcommands, int* is_background, int* is_output, int* is_output_append, char** output_file)
{
	// Set up
	char background_character = '&';
	*is_background = 0;
	char output_file_character = '>';
	*is_output = 0;
	char* output_append_character = ">>";
	*is_output_append = 0;
	int count_number_command_tokens = 0;
	int number_buffer_space = 10;
	char** input_commands = (char **)malloc(sizeof(char *) * number_buffer_space);
	if(!input_commands)
	{
		perror("input_commands allocation fail");
		return NULL;
	}
	char* cut_tokens = strtok(input_line,"|");
	while(cut_tokens)
	{
		// Cut and save the tokens for the following processing
		input_commands[count_number_command_tokens] = cut_tokens;
		count_number_command_tokens ++;
		// Reallocate the input_commands if the buffer size is too small 
		if(count_number_command_tokens + 1 >= number_buffer_space)
		{
			number_buffer_space = number_buffer_space * 2;
			input_commands = (char **)realloc(input_commands, sizeof(char *) * count_number_command_tokens);
			// Reallocation check
			if(!input_commands)
			{
				perror("reallocate input_commands fail");
				return NULL;
			}
		}
		cut_tokens = strtok(NULL, "|");
	}
	// Commands is end
	input_commands[count_number_command_tokens] = NULL;
	// Parse each sub commmand string to get the commands.
	char *** commands = (char***) malloc(sizeof(char **) * count_number_command_tokens);
	// Allocation check
	if(!commands)
	{
		perror("commands allocation fail");
		return NULL;
	}
	// Parse each subcommand tokens
	int i = 0;
	for(i = 0; i < count_number_command_tokens; i++)
	{
        // Check the last token if it is a background character or should be output redirected.
		if(i == count_number_command_tokens - 1)
		{
			// Check for background symbol
			if(strchr(input_commands[count_number_command_tokens - 1],background_character))
			{
				*is_background = 1;
			}
			if(strchr(input_commands[count_number_command_tokens - 1],output_file_character) || *is_background)
			{
				// Check for output append redirection
				if(strstr(input_commands[count_number_command_tokens - 1],output_append_character))
				{
					*is_output_append = 1;
				}
				// Check for output redirection
				else if(strchr(input_commands[count_number_command_tokens - 1],output_file_character))
				{
					*is_output = 1;
				}                                       
				// Separate the output file name with space
				char* separated_filename_withspace = NULL;
				char* cut_subtokens = strtok(input_commands[count_number_command_tokens - 1],">&\n");
				// Get the redirected output file
	        	        while(cut_subtokens)
						{
        	        	        separated_filename_withspace = cut_subtokens;
                        		cut_subtokens = strtok(NULL, ">&\n");
                		}				
				if(*is_output || *is_output_append)
				{
					// Remove space
					cut_subtokens = strtok(separated_filename_withspace, " ");
					while(cut_subtokens)
					{
						*output_file = cut_subtokens;
						cut_subtokens = strtok(NULL, " ");
					}
				}
			}
		}
		// Get the command name and arguments
		commands[i] = parse_subcommand_tokens(input_commands[i]);
		if(!commands[i])
		{
			return NULL;
		}
	}
	// Get the number of subcommands for further processing.
	*number_subcommands = count_number_command_tokens;

	return commands;	
}

int main()
{
	// Set up 
	char*** commands = NULL;
	char* input_line = NULL;
	char* output_file = NULL;
	int is_background;
	int is_output;
	int is_output_append;
	int number_subcommands;
	int execution;
	// Profrssor instruction: size now can up more than 1000
	size_t buffer_size = 1000;  
	size_t number_chars;
	
	while(1)
	{
		// My prompt
		printf("ChuAn Lab2>");
		// Flush the output as printf doesn't have \n
		fflush(stdout);
		// Preallocate the space for input
		input_line = (char*)malloc(sizeof(char) * buffer_size);
		// Memory allocation check
		if(!input_line)
		{
			perror("Space unable to be allocated!");
			exit(1);
		}
		// Read the line
		number_chars = getline(&input_line, &buffer_size, stdin);
		// Check \n
		if(!strcmp(input_line,"\n"))
		{
			continue;
		}
		// Check Exit
		if(!strcmp(input_line, "exit\n"))
		{
			exit(0);
		}		
		// Check the number of charactors from input
		if(number_chars<0)
		{
			printf("Read input error\n");
			continue;
		}
		// Parse the read line from stdin
		commands = parse_input(input_line, number_chars, &number_subcommands, &is_background, &is_output, &is_output_append, &output_file);
		if(!commands)
		{
			printf("syntax error, please enter again\n");
			continue;
		}
		execution = actual_execution(commands, number_subcommands, is_background, is_output, is_output_append, output_file);
		if(execution < 0)
		{
			return -1;
		}
	}
	// Clean up
	free(input_line);
}

/**
 * @file interpreter.cpp
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief The hyperdbg command interpreter and driver connector
 * @details
 * @version 0.1
 * @date 2020-04-11
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

using namespace std;

//
// Global Variables
//
extern ACTIVE_DEBUGGING_PROCESS g_ActiveProcessDebuggingState;
extern CommandType              g_CommandsList;

extern BOOLEAN g_ShouldPreviousCommandBeContinued;
extern BOOLEAN g_IsCommandListInitialized;
extern BOOLEAN g_LogOpened;
extern BOOLEAN g_ExecutingScript;
extern BOOLEAN g_IsConnectedToHyperDbgLocally;
extern BOOLEAN g_IsConnectedToRemoteDebuggee;
extern BOOLEAN g_IsSerialConnectedToRemoteDebuggee;
extern BOOLEAN g_IsDebuggeeRunning;
extern BOOLEAN g_BreakPrintingOutput;
extern BOOLEAN g_IsInterpreterOnString;
extern BOOLEAN g_IsInterpreterPreviousCharacterABackSlash;
extern BOOLEAN g_RtmSupport;

extern UINT32 g_VirtualAddressWidth;
extern UINT32 g_InterpreterCountOfOpenCurlyBrackets;
extern ULONG  g_CurrentRemoteCore;

extern string g_ServerPort;
extern string g_ServerIp;

class CommandParser
{
public:
    /**
     * @brief Get hex number
     * @param str
     * @return BOOL
     */
    BOOL GetHexNum(std::string & str)
    {
        if (str.empty() || (str.size() == 1 && str[0] == '0'))
            return FALSE;

        std::string Prefix("0x");

        if (!str.compare(0, Prefix.size(), Prefix))
        {
            std::string num(str.substr(Prefix.size()));

            try
            {
                size_t pos;
                auto   ul = std::stoul(num, &pos, 16);
                str       = num; // modify
                return TRUE;
            }
            catch (...)
            {
                return FALSE;
            }
        }
        else
        {
            try
            {
                size_t pos;
                auto   ul = std::stoul(str, &pos, 16);
                return TRUE;
            }
            catch (...)
            {
                return FALSE;
            }
        }
    }

    /**
     * @brief Is Decimal Number
     * @brief modifies str to actual number in string
     * @param str
     *
     * @return BOOL
     */
    BOOL IsDecNum(std::string & str)
    {
        if (str.empty() || (str.size() == 1 && str[0] == '0') || str.size() < 3)
            return FALSE;

        std::string Prefix("0n");
        std::string num(str.substr(Prefix.size()));

        if (!str.compare(0, Prefix.size(), Prefix))
        {
            try
            {
                size_t pos;
                auto   ul = std::stoul(num, &pos, 10);
                str       = num; // modify

                return TRUE;
            }
            catch (...)
            {
                return FALSE;
            }
        }

        return FALSE;
    }

    /**
     * @brief Parse the input string (commands)
     * @param input
     *
     * @return std::vector<CommandToken>
     */
    std::vector<CommandToken> Parse(const std::string & input)
    {
        std::vector<CommandToken> tokens;
        std::string               current;
        bool                      InQuotes  = FALSE;
        bool                      InBracket = FALSE;

        for (size_t i = 0; i < input.length(); ++i)
        {
            char c = input[i];
            if (c == '/') // start comment parse
            {
                //
                // if we're in a script braket, skip; it'll be handled later
                //
                if (!tokens.empty() && std::get<1>(tokens.back()) == "script")
                {
                    size_t j = i;
                    c        = input[++j];

                    if (c == '/') // start to look fo comments
                    {
                        size_t EndPose = input.find('\n', i);
                        if (EndPose != std::string::npos)
                        {
                            //
                            // here we could get the comment but for now we just skip
                            //
                            std::string comment(input.substr(i, EndPose - i));

                            i = i + (EndPose - i);

                            if (input[i + 1] == ' ' || input[i + 1] == '\n') // handling " " and "\n"
                            {
                                i++;
                                if (input[i + 1] == ' ' || input[i + 1] == '\n')
                                {
                                    i++;
                                }
                            }

                            continue;
                        }
                    }
                    else if (c == '*')
                    {
                        size_t EndPose = input.find("*/", i);

                        if (EndPose != std::string::npos)
                        {
                            //
                            // here we could get the comment but for now we just skip
                            //
                            std::string comment(input.substr(i, EndPose - i + 2)); // */ is two byte long

                            i = (i + (EndPose - i)) + 1; // one for /

                            if (input[i + 1] == ' ' || input[i + 1] == '\n') // handling " " and "\n"
                            {
                                i++;

                                if (input[i + 1] == ' ' || input[i + 1] == '\n')
                                {
                                    i++;
                                }
                            }

                            continue;
                        }
                    }
                }
            }

            if (InQuotes)
            {
                if (c == '"' && input[i - 1] != '\\' && !InBracket)
                {
                    if (input[i + 1] == ' ' || input[i + 1] == '\n') // handling " " and "\n"
                    {
                        i++;

                        if (input[i + 1] == ' ' || input[i + 1] == '\n')
                        {
                            i++;
                        }
                    }

                    InQuotes = FALSE;
                    tokens.emplace_back(CommandParsingTokenType::String, current, ToLower(current));
                    current.clear();

                    continue;
                }
            }

            if (InBracket)
            {
                if (c == '}')
                {
                    if (input[i + 1] == ' ' || input[i + 1] == '\n') // handling " " and "\n"
                    {
                        i++;
                        if (input[i + 1] == ' ' || input[i + 1] == '\n')
                        {
                            i++;
                        }
                    }

                    InBracket = FALSE;
                    tokens.emplace_back(CommandParsingTokenType::BracketString, current, ToLower(current));
                    current.clear();

                    continue;
                }
            }

            if (c == ' ' && !InQuotes && !InBracket)
            {
                if (!current.empty() && current != " ")
                {
                    AddToken(tokens, current);
                    current.clear();

                    continue;
                }
            }
            else if (c == '"' && input[i - 1] != '\\' && !InBracket)
            {
                InQuotes = TRUE;
                continue; // don't include '"' in string
            }
            else if (c == '{' && !InQuotes && !InBracket)
            {
                InBracket = TRUE;
                continue; // don't include '{' in string
            }

            current += c;
        }

        if (!current.empty() && current != " ")
        {
            AddToken(tokens, current);
        }

        return tokens;
    }

    /**
     * @brief Function to convert CommandParsingTokenType to a string
     * @param Type
     *
     * @return std::string
     */
    std::string TokenTypeToString(CommandParsingTokenType Type)
    {
        switch (Type)
        {
        case CommandParsingTokenType::NumHex:
            return "NumHex";

        case CommandParsingTokenType::NumDec:
            return "NumDec";

        case CommandParsingTokenType::String:
            return "String";

        case CommandParsingTokenType::BracketString:
            return "BracketString";

        default:
            return "Unknown";
        }
    }

    /**
     * @brief Function to print the elements of a vector of Tokens
     * @param Tokens
     *
     * @return VOID
     */
    VOID PrintTokens(const std::vector<CommandToken> & Tokens)
    {
        for (const auto & Token : Tokens)
        {
            ShowMessages("CommandParsingTokenType: %s , Value 1: %s, Value 2 (lower): %s\n",
                         TokenTypeToString(std::get<0>(Token)).c_str(),
                         std::get<1>(Token).c_str(),
                         std::get<2>(Token).c_str());
        }
    }

private:
    /**
     * @brief Convert a string to lowercase
     * @param str
     *
     * @return std::string
     */
    std::string ToLower(const std::string & str) const
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);

        return result;
    }

    /**
     * @brief Add Token
     * @param tokens
     * @param str
     *
     * @return VOID
     */
    void AddToken(std::vector<CommandToken> & tokens, const std::string & str)
    {
        auto tmp = str;

        if (IsDecNum(tmp)) // tmp will be modified to actual number
        {
            tokens.emplace_back(CommandParsingTokenType::NumDec, tmp, ToLower(tmp));
        }
        else if (std::all_of(str.begin(), str.end(), ::isdigit) || GetHexNum(tmp)) // tmp will be modified to actual number
        {
            tokens.emplace_back(CommandParsingTokenType::NumHex, tmp, ToLower(tmp));
        }
        else
        {
            tokens.emplace_back(CommandParsingTokenType::String, str, ToLower(str));
        }
    }
};

/**
 * @brief Interpret commands
 *
 * @param Command The text of command
 * @return INT returns return zero if it was successful or non-zero if there was
 * error
 */
INT
HyperDbgInterpreter(CHAR * Command)
{
    BOOLEAN               HelpCommand       = FALSE;
    UINT64                CommandAttributes = NULL;
    CommandType::iterator Iterator;

    //
    // Check if it's the first command and whether the mapping of command is
    // initialized or not
    //
    if (!g_IsCommandListInitialized)
    {
        //
        // Initialize the debugger
        //
        InitializeDebugger();

        g_IsCommandListInitialized = TRUE;
    }

    //
    // Save the command into log open file
    //
    if (g_LogOpened && !g_ExecutingScript)
    {
        LogopenSaveToFile(Command);
        LogopenSaveToFile("\n");
    }

    //
    // Remove the comments
    //
    InterpreterRemoveComments(Command);

    //
    // Convert to string
    //
    string CommandString(Command);

    //
    // Tokenize the command string
    //
    CommandParser Parser;
    auto          Tokens = Parser.Parse(CommandString);

    ShowMessages("------------------------------------------------------\n");

    //
    // Print the tokens
    //
    Parser.PrintTokens(Tokens);

    ShowMessages("\n------------------------------------------------------\n");

    //
    // Convert to lower case
    //
    transform(CommandString.begin(), CommandString.end(), CommandString.begin(), [](unsigned char c) { return std::tolower(c); });

    vector<string> SplitCommand {Split(CommandString, ' ')};

    //
    // Check if user entered an empty input
    //
    if (SplitCommand.empty())
    {
        ShowMessages("\n");
        return 0;
    }

    string FirstCommand = SplitCommand.front();

    //
    // Read the command's attributes
    //
    CommandAttributes = GetCommandAttributes(FirstCommand);

    //
    // Check if the command needs to be continued by pressing enter
    //
    if (CommandAttributes & DEBUGGER_COMMAND_ATTRIBUTE_REPEAT_ON_ENTER)
    {
        g_ShouldPreviousCommandBeContinued = TRUE;
    }
    else
    {
        g_ShouldPreviousCommandBeContinued = FALSE;
    }

    //
    // Check and send remote command and also we check whether this
    // is a command that should be handled in this command or we can
    // send it to the remote computer, it is because in a remote connection
    // still some of the commands should be handled in the local HyperDbg
    //
    if (g_IsConnectedToRemoteDebuggee &&
        !(CommandAttributes & DEBUGGER_COMMAND_ATTRIBUTE_LOCAL_COMMAND_IN_REMOTE_CONNECTION))
    {
        //
        // Check it here because generally, we use this variable in host
        // for showing the correct signature but we won't try to block
        // other commands, the only thing is events which is blocked
        // by the remote computer itself
        //
        if (g_BreakPrintingOutput)
        {
            g_BreakPrintingOutput = FALSE;
        }

        //
        // It's a connection over network (VMI-Mode)
        //
        RemoteConnectionSendCommand(Command, (UINT32)strlen(Command) + 1);

        ShowMessages("\n");

        //
        // Indicate that we sent the command to the target system
        //
        return 2;
    }
    else if (g_IsSerialConnectedToRemoteDebuggee &&
             !(CommandAttributes &
               DEBUGGER_COMMAND_ATTRIBUTE_LOCAL_COMMAND_IN_DEBUGGER_MODE))
    {
        //
        // It's a connection over serial (Debugger-Mode)
        //

        if (CommandAttributes & DEBUGGER_COMMAND_ATTRIBUTE_WONT_STOP_DEBUGGER_AGAIN)
        {
            KdSendUserInputPacketToDebuggee(Command, (UINT32)strlen(Command) + 1, TRUE);

            //
            // Set the debuggee to show that it's running
            //
            KdSetStatusAndWaitForPause();
        }
        else
        {
            //
            // Disable the breakpoints and events while executing the command in the remote computer
            //
            KdSendTestQueryPacketToDebuggee(TEST_BREAKPOINT_TURN_OFF_BPS_AND_EVENTS_FOR_COMMANDS_IN_REMOTE_COMPUTER);
            KdSendUserInputPacketToDebuggee(Command, (UINT32)strlen(Command) + 1, FALSE);
            KdSendTestQueryPacketToDebuggee(TEST_BREAKPOINT_TURN_ON_BPS_AND_EVENTS_FOR_COMMANDS_IN_REMOTE_COMPUTER);
        }

        //
        // Indicate that we sent the command to the target system
        //
        return 2;
    }

    //
    // Detect whether it's a .help command or not
    //
    if (!FirstCommand.compare(".help") || !FirstCommand.compare("help") ||
        !FirstCommand.compare(".hh"))
    {
        if (SplitCommand.size() == 2)
        {
            //
            // Show that it's a help command
            //
            HelpCommand  = TRUE;
            FirstCommand = SplitCommand.at(1);
        }
        else
        {
            ShowMessages("incorrect use of the '%s'\n", FirstCommand.c_str());
            CommandHelpHelp();
            return 0;
        }
    }

    //
    // Start parsing commands
    //
    Iterator = g_CommandsList.find(FirstCommand);

    if (Iterator == g_CommandsList.end())
    {
        //
        //  Command doesn't exist
        //
        string         CaseSensitiveCommandString(Command);
        vector<string> CaseSensitiveSplitCommand {Split(CaseSensitiveCommandString, ' ')};

        if (!HelpCommand)
        {
            ShowMessages("err, couldn't resolve command at '%s'\n", CaseSensitiveSplitCommand.front().c_str());
        }
        else
        {
            ShowMessages("err, couldn't find the help for the command at '%s'\n",
                         CaseSensitiveSplitCommand.at(1).c_str());
        }
    }
    else
    {
        if (HelpCommand)
        {
            Iterator->second.CommandHelpFunction();
        }
        else
        {
            //
            // Check if command is case-sensitive or not
            //
            if ((Iterator->second.CommandFunctionNewParser != NULL))
            {
                //
                // Call the parser with tokens
                //
                Iterator->second.CommandFunctionNewParser(Tokens);
            }
            else if ((Iterator->second.CommandAttrib &
                      DEBUGGER_COMMAND_ATTRIBUTE_LOCAL_CASE_SENSITIVE))
            {
                string CaseSensitiveCommandString(Command);
                Iterator->second.CommandFunction(SplitCommand, CaseSensitiveCommandString);
            }
            else
            {
                Iterator->second.CommandFunction(SplitCommand, CommandString);
            }
        }
    }

    //
    // Save the command into log open file
    //
    if (g_LogOpened && !g_ExecutingScript)
    {
        LogopenSaveToFile("\n");
    }

    return 0;
}

/**
 * @brief Remove batch comments
 *
 * @return VOID
 */
VOID
InterpreterRemoveComments(char * CommandText)
{
    BOOLEAN IsComment       = FALSE;
    BOOLEAN IsOnString      = FALSE;
    UINT32  LengthOfCommand = (UINT32)strlen(CommandText);

    for (size_t i = 0; i < LengthOfCommand; i++)
    {
        if (IsComment)
        {
            if (CommandText[i] == '\n')
            {
                IsComment = FALSE;
            }
            else
            {
                if (CommandText[i] != '\0')
                {
                    memmove((void *)&CommandText[i], (const void *)&CommandText[i + 1], strlen(CommandText) - i);
                    i--;
                }
            }
        }
        else if (CommandText[i] == '#' && !IsOnString)
        {
            //
            // Comment detected
            //
            IsComment = TRUE;
            i--;
        }
        else if (CommandText[i] == '"')
        {
            if (i != 0 && CommandText[i - 1] == '\\')
            {
                //
                // It's an escape character for : \"
                //
            }
            else if (IsOnString)
            {
                IsOnString = FALSE;
            }
            else
            {
                IsOnString = TRUE;
            }
        }
    }
}

/**
 * @brief Show signature of HyperDbg
 *
 * @return VOID
 */
VOID
HyperDbgShowSignature()
{
    if (g_IsConnectedToRemoteDebuggee)
    {
        //
        // Remote debugging over tcp (vmi-mode)
        //
        ShowMessages("[%s:%s] HyperDbg> ", g_ServerIp.c_str(), g_ServerPort.c_str());
    }
    else if (g_ActiveProcessDebuggingState.IsActive)
    {
        //
        // Debugging a special process
        //
        ShowMessages("%x:%x u%sHyperDbg> ",
                     g_ActiveProcessDebuggingState.ProcessId,
                     g_ActiveProcessDebuggingState.ThreadId,
                     g_ActiveProcessDebuggingState.Is32Bit ? "86" : "64");
    }
    else if (g_IsSerialConnectedToRemoteDebuggee)
    {
        //
        // Remote debugging over serial (debugger-mode)
        //
        ShowMessages("%x: kHyperDbg> ", g_CurrentRemoteCore);
    }
    else
    {
        //
        // Anything other than above scenarios including local debugging
        // in vmi-mode
        //
        ShowMessages("HyperDbg> ");
    }
}

/**
 * @brief check for multi-line commands
 *
 * @param CurrentCommand
 * @param Reset
 * @return BOOLEAN return TRUE if the command needs extra input, otherwise
 * return FALSE
 */
BOOLEAN
CheckMultilineCommand(CHAR * CurrentCommand, BOOLEAN Reset)
{
    UINT32      CurrentCommandLen = 0;
    std::string CurrentCommandStr(CurrentCommand);

    if (Reset)
    {
        g_IsInterpreterOnString                    = FALSE;
        g_IsInterpreterPreviousCharacterABackSlash = FALSE;
        g_InterpreterCountOfOpenCurlyBrackets      = 0;
    }

    CurrentCommandLen = (UINT32)CurrentCommandStr.length();

    for (size_t i = 0; i < CurrentCommandLen; i++)
    {
        switch (CurrentCommandStr.at(i))
        {
        case '"':

            if (g_IsInterpreterPreviousCharacterABackSlash)
            {
                g_IsInterpreterPreviousCharacterABackSlash = FALSE;
                break; // it's an escaped \" double-quote
            }

            if (g_IsInterpreterOnString)
                g_IsInterpreterOnString = FALSE;
            else
                g_IsInterpreterOnString = TRUE;

            break;

        case '{':

            if (g_IsInterpreterPreviousCharacterABackSlash)
                g_IsInterpreterPreviousCharacterABackSlash = FALSE;

            if (!g_IsInterpreterOnString)
                g_InterpreterCountOfOpenCurlyBrackets++;

            break;

        case '}':

            if (g_IsInterpreterPreviousCharacterABackSlash)
                g_IsInterpreterPreviousCharacterABackSlash = FALSE;

            if (!g_IsInterpreterOnString && g_InterpreterCountOfOpenCurlyBrackets > 0)
                g_InterpreterCountOfOpenCurlyBrackets--;

            break;

        case '\\':

            if (g_IsInterpreterPreviousCharacterABackSlash)
                g_IsInterpreterPreviousCharacterABackSlash = FALSE; // it's not a escape character (two backslashes \\ )
            else
                g_IsInterpreterPreviousCharacterABackSlash = TRUE;

            break;

        default:

            if (g_IsInterpreterPreviousCharacterABackSlash)
                g_IsInterpreterPreviousCharacterABackSlash = FALSE;

            break;
        }
    }

    if (g_IsInterpreterOnString == FALSE && g_InterpreterCountOfOpenCurlyBrackets == 0)
    {
        //
        // either the command is finished or it's a single
        // line command
        //
        return FALSE;
    }
    else
    {
        //
        // There still other lines, this command is incomplete
        //
        return TRUE;
    }
}

/**
 * @brief Some of commands like stepping commands (i, p, t) and etc.
 * need to be repeated when the user press enter, this function shows
 * whether we should continue the previous command or not
 *
 * @return TRUE means the command should be continued, FALSE means command
 * should be ignored
 */
BOOLEAN
ContinuePreviousCommand()
{
    BOOLEAN Result = g_ShouldPreviousCommandBeContinued;

    //
    // We should keep it false for the next command
    //
    g_ShouldPreviousCommandBeContinued = FALSE;

    if (Result)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**
 * @brief Get Command Attributes
 *
 * @param FirstCommand just the first word of command (without other parameters)
 * @return BOOLEAN Mask of the command's attributes
 */
UINT64
GetCommandAttributes(const string & FirstCommand)
{
    CommandType::iterator Iterator;

    //
    // Some commands should not be passed to the remote system
    // and instead should be handled in the current debugger
    //

    Iterator = g_CommandsList.find(FirstCommand);

    if (Iterator == g_CommandsList.end())
    {
        //
        // Command doesn't exist, if it's not exists then it's better to handle
        // it locally, instead of sending it to the remote computer
        //
        return DEBUGGER_COMMAND_ATTRIBUTE_ABSOLUTE_LOCAL;
    }
    else
    {
        return Iterator->second.CommandAttrib;
    }

    return NULL;
}

/**
 * @brief Initialize the debugger and adjust commands for the first run
 *
 * @return VOID
 */
VOID
InitializeDebugger()
{
    //
    // Initialize the mapping of functions
    //
    InitializeCommandsDictionary();

    //
    // Set the callback for symbol message handler
    //
    ScriptEngineSetTextMessageCallbackWrapper(ShowMessages);

    //
    // Register the CTRL+C and CTRL+BREAK Signals handler
    //
    if (!SetConsoleCtrlHandler(BreakController, TRUE))
    {
        ShowMessages("err, when registering CTRL+C and CTRL+BREAK Signals "
                     "handler\n");
        //
        // prefer to continue
        //
    }

    //
    // *** Check for feature indicators ***
    //

    //
    // Get x86 processor width for virtual address
    //
    g_VirtualAddressWidth = Getx86VirtualAddressWidth();

    //
    // Check if processor supports TSX (RTM)
    //
    g_RtmSupport = CheckCpuSupportRtm();

    //
    // Load default settings
    //
    CommandSettingsLoadDefaultValuesFromConfigFile();
}

/**
 * @brief Initialize commands and attributes
 *
 * @return VOID
 */
VOID
InitializeCommandsDictionary()
{
    g_CommandsList[".help"] = {NULL, NULL, &CommandHelpHelp, DEBUGGER_COMMAND_HELP_ATTRIBUTES};
    g_CommandsList[".hh"]   = {NULL, NULL, &CommandHelpHelp, DEBUGGER_COMMAND_HELP_ATTRIBUTES};
    g_CommandsList["help"]  = {NULL, NULL, &CommandHelpHelp, DEBUGGER_COMMAND_HELP_ATTRIBUTES};

    g_CommandsList["clear"] = {NULL, &CommandClearScreen, &CommandClearScreenHelp, DEBUGGER_COMMAND_CLEAR_ATTRIBUTES};
    g_CommandsList[".cls"]  = {NULL, &CommandClearScreen, &CommandClearScreenHelp, DEBUGGER_COMMAND_CLEAR_ATTRIBUTES};
    g_CommandsList["cls"]   = {NULL, &CommandClearScreen, &CommandClearScreenHelp, DEBUGGER_COMMAND_CLEAR_ATTRIBUTES};

    g_CommandsList[".connect"] = {NULL, &CommandConnect, &CommandConnectHelp, DEBUGGER_COMMAND_CONNECT_ATTRIBUTES};
    g_CommandsList["connect"]  = {NULL, &CommandConnect, &CommandConnectHelp, DEBUGGER_COMMAND_CONNECT_ATTRIBUTES};

    g_CommandsList[".listen"] = {NULL, &CommandListen, &CommandListenHelp, DEBUGGER_COMMAND_LISTEN_ATTRIBUTES};
    g_CommandsList["listen"]  = {NULL, &CommandListen, &CommandListenHelp, DEBUGGER_COMMAND_LISTEN_ATTRIBUTES};

    g_CommandsList["g"]  = {&CommandG, NULL, &CommandGHelp, DEBUGGER_COMMAND_G_ATTRIBUTES};
    g_CommandsList["go"] = {&CommandG, NULL, &CommandGHelp, DEBUGGER_COMMAND_G_ATTRIBUTES};

    g_CommandsList[".attach"] = {NULL, &CommandAttach, &CommandAttachHelp, DEBUGGER_COMMAND_ATTACH_ATTRIBUTES};
    g_CommandsList["attach"]  = {NULL, &CommandAttach, &CommandAttachHelp, DEBUGGER_COMMAND_ATTACH_ATTRIBUTES};

    g_CommandsList[".detach"] = {NULL, &CommandDetach, &CommandDetachHelp, DEBUGGER_COMMAND_DETACH_ATTRIBUTES};
    g_CommandsList["detach"]  = {NULL, &CommandDetach, &CommandDetachHelp, DEBUGGER_COMMAND_DETACH_ATTRIBUTES};

    g_CommandsList[".start"] = {NULL, &CommandStart, &CommandStartHelp, DEBUGGER_COMMAND_START_ATTRIBUTES};
    g_CommandsList["start"]  = {NULL, &CommandStart, &CommandStartHelp, DEBUGGER_COMMAND_START_ATTRIBUTES};

    g_CommandsList[".restart"] = {NULL, &CommandRestart, &CommandRestartHelp, DEBUGGER_COMMAND_RESTART_ATTRIBUTES};
    g_CommandsList["restart"]  = {NULL, &CommandRestart, &CommandRestartHelp, DEBUGGER_COMMAND_RESTART_ATTRIBUTES};

    g_CommandsList[".switch"] = {NULL, &CommandSwitch, &CommandSwitchHelp, DEBUGGER_COMMAND_SWITCH_ATTRIBUTES};
    g_CommandsList["switch"]  = {NULL, &CommandSwitch, &CommandSwitchHelp, DEBUGGER_COMMAND_SWITCH_ATTRIBUTES};

    g_CommandsList[".kill"] = {NULL, &CommandKill, &CommandKillHelp, DEBUGGER_COMMAND_KILL_ATTRIBUTES};
    g_CommandsList["kill"]  = {NULL, &CommandKill, &CommandKillHelp, DEBUGGER_COMMAND_KILL_ATTRIBUTES};

    g_CommandsList[".process"]  = {NULL, &CommandProcess, &CommandProcessHelp, DEBUGGER_COMMAND_PROCESS_ATTRIBUTES};
    g_CommandsList[".process2"] = {NULL, &CommandProcess, &CommandProcessHelp, DEBUGGER_COMMAND_PROCESS_ATTRIBUTES};
    g_CommandsList["process"]   = {NULL, &CommandProcess, &CommandProcessHelp, DEBUGGER_COMMAND_PROCESS_ATTRIBUTES};
    g_CommandsList["process2"]  = {NULL, &CommandProcess, &CommandProcessHelp, DEBUGGER_COMMAND_PROCESS_ATTRIBUTES};

    g_CommandsList[".thread"]  = {NULL, &CommandThread, &CommandThreadHelp, DEBUGGER_COMMAND_THREAD_ATTRIBUTES};
    g_CommandsList[".thread2"] = {NULL, &CommandThread, &CommandThreadHelp, DEBUGGER_COMMAND_THREAD_ATTRIBUTES};
    g_CommandsList["thread"]   = {NULL, &CommandThread, &CommandThreadHelp, DEBUGGER_COMMAND_THREAD_ATTRIBUTES};
    g_CommandsList["thread2"]  = {NULL, &CommandThread, &CommandThreadHelp, DEBUGGER_COMMAND_THREAD_ATTRIBUTES};

    g_CommandsList["sleep"] = {NULL, &CommandSleep, &CommandSleepHelp, DEBUGGER_COMMAND_SLEEP_ATTRIBUTES};

    g_CommandsList["event"]  = {NULL, &CommandEvents, &CommandEventsHelp, DEBUGGER_COMMAND_EVENTS_ATTRIBUTES};
    g_CommandsList["events"] = {NULL, &CommandEvents, &CommandEventsHelp, DEBUGGER_COMMAND_EVENTS_ATTRIBUTES};

    g_CommandsList["setting"]   = {NULL, &CommandSettings, &CommandSettingsHelp, DEBUGGER_COMMAND_SETTINGS_ATTRIBUTES};
    g_CommandsList["settings"]  = {NULL, &CommandSettings, &CommandSettingsHelp, DEBUGGER_COMMAND_SETTINGS_ATTRIBUTES};
    g_CommandsList[".setting"]  = {NULL, &CommandSettings, &CommandSettingsHelp, DEBUGGER_COMMAND_SETTINGS_ATTRIBUTES};
    g_CommandsList[".settings"] = {NULL, &CommandSettings, &CommandSettingsHelp, DEBUGGER_COMMAND_SETTINGS_ATTRIBUTES};

    g_CommandsList[".disconnect"] = {NULL, &CommandDisconnect, &CommandDisconnectHelp, DEBUGGER_COMMAND_DISCONNECT_ATTRIBUTES};
    g_CommandsList["disconnect"]  = {NULL, &CommandDisconnect, &CommandDisconnectHelp, DEBUGGER_COMMAND_DISCONNECT_ATTRIBUTES};

    g_CommandsList[".debug"] = {NULL, &CommandDebug, &CommandDebugHelp, DEBUGGER_COMMAND_DEBUG_ATTRIBUTES};
    g_CommandsList["debug"]  = {NULL, &CommandDebug, &CommandDebugHelp, DEBUGGER_COMMAND_DEBUG_ATTRIBUTES};

    g_CommandsList[".status"] = {NULL, &CommandStatus, &CommandStatusHelp, DEBUGGER_COMMAND_DOT_STATUS_ATTRIBUTES};
    g_CommandsList["status"]  = {NULL, &CommandStatus, &CommandStatusHelp, DEBUGGER_COMMAND_STATUS_ATTRIBUTES};

    g_CommandsList["load"] = {NULL, &CommandLoad, &CommandLoadHelp, DEBUGGER_COMMAND_LOAD_ATTRIBUTES};

    g_CommandsList["exit"]  = {NULL, &CommandExit, &CommandExitHelp, DEBUGGER_COMMAND_EXIT_ATTRIBUTES};
    g_CommandsList[".exit"] = {NULL, &CommandExit, &CommandExitHelp, DEBUGGER_COMMAND_EXIT_ATTRIBUTES};

    g_CommandsList["flush"] = {NULL, &CommandFlush, &CommandFlushHelp, DEBUGGER_COMMAND_FLUSH_ATTRIBUTES};

    g_CommandsList["pause"] = {NULL, &CommandPause, &CommandPauseHelp, DEBUGGER_COMMAND_PAUSE_ATTRIBUTES};

    g_CommandsList["unload"] = {NULL, &CommandUnload, &CommandUnloadHelp, DEBUGGER_COMMAND_UNLOAD_ATTRIBUTES};

    g_CommandsList[".script"] = {NULL, &CommandScript, &CommandScriptHelp, DEBUGGER_COMMAND_SCRIPT_ATTRIBUTES};
    g_CommandsList["script"]  = {NULL, &CommandScript, &CommandScriptHelp, DEBUGGER_COMMAND_SCRIPT_ATTRIBUTES};

    g_CommandsList["output"] = {NULL, &CommandOutput, &CommandOutputHelp, DEBUGGER_COMMAND_OUTPUT_ATTRIBUTES};

    g_CommandsList["print"] = {NULL, &CommandPrint, &CommandPrintHelp, DEBUGGER_COMMAND_PRINT_ATTRIBUTES};

    g_CommandsList["?"]        = {NULL, &CommandEval, &CommandEvalHelp, DEBUGGER_COMMAND_EVAL_ATTRIBUTES};
    g_CommandsList["eval"]     = {NULL, &CommandEval, &CommandEvalHelp, DEBUGGER_COMMAND_EVAL_ATTRIBUTES};
    g_CommandsList["evaluate"] = {NULL, &CommandEval, &CommandEvalHelp, DEBUGGER_COMMAND_EVAL_ATTRIBUTES};

    g_CommandsList[".logopen"] = {NULL, &CommandLogopen, &CommandLogopenHelp, DEBUGGER_COMMAND_LOGOPEN_ATTRIBUTES};

    g_CommandsList[".logclose"] = {NULL, &CommandLogclose, &CommandLogcloseHelp, DEBUGGER_COMMAND_LOGCLOSE_ATTRIBUTES};

    g_CommandsList[".pagein"] = {NULL, &CommandPagein, &CommandPageinHelp, DEBUGGER_COMMAND_PAGEIN_ATTRIBUTES};
    g_CommandsList["pagein"]  = {NULL, &CommandPagein, &CommandPageinHelp, DEBUGGER_COMMAND_PAGEIN_ATTRIBUTES};

    g_CommandsList["test"] = {NULL, &CommandTest, &CommandTestHelp, DEBUGGER_COMMAND_TEST_ATTRIBUTES};

    g_CommandsList["cpu"] = {NULL, &CommandCpu, &CommandCpuHelp, DEBUGGER_COMMAND_CPU_ATTRIBUTES};

    g_CommandsList["wrmsr"] = {NULL, &CommandWrmsr, &CommandWrmsrHelp, DEBUGGER_COMMAND_WRMSR_ATTRIBUTES};

    g_CommandsList["rdmsr"] = {NULL, &CommandRdmsr, &CommandRdmsrHelp, DEBUGGER_COMMAND_RDMSR_ATTRIBUTES};

    g_CommandsList["!va2pa"] = {NULL, &CommandVa2pa, &CommandVa2paHelp, DEBUGGER_COMMAND_VA2PA_ATTRIBUTES};

    g_CommandsList["!pa2va"] = {NULL, &CommandPa2va, &CommandPa2vaHelp, DEBUGGER_COMMAND_PA2VA_ATTRIBUTES};

    g_CommandsList[".formats"] = {NULL, &CommandFormats, &CommandFormatsHelp, DEBUGGER_COMMAND_FORMATS_ATTRIBUTES};
    g_CommandsList[".format"]  = {NULL, &CommandFormats, &CommandFormatsHelp, DEBUGGER_COMMAND_FORMATS_ATTRIBUTES};

    g_CommandsList["!pte"] = {NULL, &CommandPte, &CommandPteHelp, DEBUGGER_COMMAND_PTE_ATTRIBUTES};

    g_CommandsList["~"]    = {&CommandCore, NULL, &CommandCoreHelp, DEBUGGER_COMMAND_CORE_ATTRIBUTES};
    g_CommandsList["core"] = {&CommandCore, NULL, &CommandCoreHelp, DEBUGGER_COMMAND_CORE_ATTRIBUTES};

    g_CommandsList["!monitor"] = {NULL, &CommandMonitor, &CommandMonitorHelp, DEBUGGER_COMMAND_MONITOR_ATTRIBUTES};

    g_CommandsList["!vmcall"] = {NULL, &CommandVmcall, &CommandVmcallHelp, DEBUGGER_COMMAND_VMCALL_ATTRIBUTES};

    g_CommandsList["!epthook"] = {NULL, &CommandEptHook, &CommandEptHookHelp, DEBUGGER_COMMAND_EPTHOOK_ATTRIBUTES};
    g_CommandsList["bh"]       = {NULL, &CommandEptHook, &CommandEptHookHelp, DEBUGGER_COMMAND_EPTHOOK_ATTRIBUTES};

    g_CommandsList["bp"] = {&CommandBp, NULL, &CommandBpHelp, DEBUGGER_COMMAND_BP_ATTRIBUTES};

    g_CommandsList["bl"] = {&CommandBl, NULL, &CommandBlHelp, DEBUGGER_COMMAND_BD_ATTRIBUTES};

    g_CommandsList["be"] = {&CommandBe, NULL, &CommandBeHelp, DEBUGGER_COMMAND_BD_ATTRIBUTES};

    g_CommandsList["bd"] = {&CommandBd, NULL, &CommandBdHelp, DEBUGGER_COMMAND_BD_ATTRIBUTES};

    g_CommandsList["bc"] = {&CommandBc, NULL, &CommandBcHelp, DEBUGGER_COMMAND_BD_ATTRIBUTES};

    g_CommandsList["!epthook2"] = {NULL, &CommandEptHook2, &CommandEptHook2Help, DEBUGGER_COMMAND_EPTHOOK2_ATTRIBUTES};

    g_CommandsList["!cpuid"] = {NULL, &CommandCpuid, &CommandCpuidHelp, DEBUGGER_COMMAND_CPUID_ATTRIBUTES};

    g_CommandsList["!msrread"] = {NULL, &CommandMsrread, &CommandMsrreadHelp, DEBUGGER_COMMAND_MSRREAD_ATTRIBUTES};
    g_CommandsList["!msread"]  = {NULL, &CommandMsrread, &CommandMsrreadHelp, DEBUGGER_COMMAND_MSRREAD_ATTRIBUTES};

    g_CommandsList["!msrwrite"] = {NULL, &CommandMsrwrite, &CommandMsrwriteHelp, DEBUGGER_COMMAND_MSRWRITE_ATTRIBUTES};

    g_CommandsList["!tsc"] = {NULL, &CommandTsc, &CommandTscHelp, DEBUGGER_COMMAND_TSC_ATTRIBUTES};

    g_CommandsList["!pmc"] = {NULL, &CommandPmc, &CommandPmcHelp, DEBUGGER_COMMAND_PMC_ATTRIBUTES};

    g_CommandsList["!crwrite"] = {NULL, &CommandCrwrite, &CommandCrwriteHelp, DEBUGGER_COMMAND_CRWRITE_ATTRIBUTES};

    g_CommandsList["!dr"] = {NULL, &CommandDr, &CommandDrHelp, DEBUGGER_COMMAND_DR_ATTRIBUTES};

    g_CommandsList["!ioin"] = {NULL, &CommandIoin, &CommandIoinHelp, DEBUGGER_COMMAND_IOIN_ATTRIBUTES};

    g_CommandsList["!ioout"] = {NULL, &CommandIoout, &CommandIooutHelp, DEBUGGER_COMMAND_IOOUT_ATTRIBUTES};
    g_CommandsList["!iout"]  = {NULL, &CommandIoout, &CommandIooutHelp, DEBUGGER_COMMAND_IOOUT_ATTRIBUTES};

    g_CommandsList["!exception"] = {NULL, &CommandException, &CommandExceptionHelp, DEBUGGER_COMMAND_EXCEPTION_ATTRIBUTES};

    g_CommandsList["!interrupt"] = {NULL, &CommandInterrupt, &CommandInterruptHelp, DEBUGGER_COMMAND_INTERRUPT_ATTRIBUTES};

    g_CommandsList["!syscall"]  = {NULL, &CommandSyscallAndSysret, &CommandSyscallHelp, DEBUGGER_COMMAND_SYSCALL_ATTRIBUTES};
    g_CommandsList["!syscall2"] = {NULL, &CommandSyscallAndSysret, &CommandSyscallHelp, DEBUGGER_COMMAND_SYSCALL_ATTRIBUTES};

    g_CommandsList["!sysret"]  = {NULL, &CommandSyscallAndSysret, &CommandSysretHelp, DEBUGGER_COMMAND_SYSRET_ATTRIBUTES};
    g_CommandsList["!sysret2"] = {NULL, &CommandSyscallAndSysret, &CommandSysretHelp, DEBUGGER_COMMAND_SYSRET_ATTRIBUTES};

    g_CommandsList["!mode"] = {NULL, &CommandMode, &CommandModeHelp, DEBUGGER_COMMAND_MODE_ATTRIBUTES};

    g_CommandsList["!trace"] = {NULL, &CommandTrace, &CommandTraceHelp, DEBUGGER_COMMAND_TRACE_ATTRIBUTES};

    g_CommandsList["!hide"] = {NULL, &CommandHide, &CommandHideHelp, DEBUGGER_COMMAND_HIDE_ATTRIBUTES};

    g_CommandsList["!unhide"] = {NULL, &CommandUnhide, &CommandUnhideHelp, DEBUGGER_COMMAND_UNHIDE_ATTRIBUTES};

    g_CommandsList["!measure"] = {NULL, &CommandMeasure, &CommandMeasureHelp, DEBUGGER_COMMAND_MEASURE_ATTRIBUTES};

    g_CommandsList["lm"] = {NULL, &CommandLm, &CommandLmHelp, DEBUGGER_COMMAND_LM_ATTRIBUTES};

    g_CommandsList["p"]  = {NULL, &CommandP, &CommandPHelp, DEBUGGER_COMMAND_P_ATTRIBUTES};
    g_CommandsList["pr"] = {NULL, &CommandP, &CommandPHelp, DEBUGGER_COMMAND_P_ATTRIBUTES};

    g_CommandsList["t"]  = {NULL, &CommandT, &CommandTHelp, DEBUGGER_COMMAND_T_ATTRIBUTES};
    g_CommandsList["tr"] = {NULL, &CommandT, &CommandTHelp, DEBUGGER_COMMAND_T_ATTRIBUTES};

    g_CommandsList["i"]  = {NULL, &CommandI, &CommandIHelp, DEBUGGER_COMMAND_I_ATTRIBUTES};
    g_CommandsList["ir"] = {NULL, &CommandI, &CommandIHelp, DEBUGGER_COMMAND_I_ATTRIBUTES};

    g_CommandsList["gu"] = {NULL, &CommandGu, &CommandGuHelp, DEBUGGER_COMMAND_GU_ATTRIBUTES};

    g_CommandsList["db"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["dc"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["dd"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["dq"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!db"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!dc"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!dd"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!dq"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!u"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["u"]    = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!u64"] = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["u64"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!u2"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["u2"]   = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["!u32"] = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};
    g_CommandsList["u32"]  = {NULL, &CommandReadMemoryAndDisassembler, &CommandReadMemoryAndDisassemblerHelp, DEBUGGER_COMMAND_D_AND_U_ATTRIBUTES};

    g_CommandsList["eb"]  = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};
    g_CommandsList["ed"]  = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};
    g_CommandsList["eq"]  = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};
    g_CommandsList["!eb"] = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};
    g_CommandsList["!ed"] = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};
    g_CommandsList["!eq"] = {NULL, &CommandEditMemory, &CommandEditMemoryHelp, DEBUGGER_COMMAND_E_ATTRIBUTES};

    g_CommandsList["sb"]  = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};
    g_CommandsList["sd"]  = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};
    g_CommandsList["sq"]  = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};
    g_CommandsList["!sb"] = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};
    g_CommandsList["!sd"] = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};
    g_CommandsList["!sq"] = {NULL, &CommandSearchMemory, &CommandSearchMemoryHelp, DEBUGGER_COMMAND_S_ATTRIBUTES};

    g_CommandsList["r"] = {NULL, &CommandR, &CommandRHelp, DEBUGGER_COMMAND_R_ATTRIBUTES};

    g_CommandsList[".sympath"] = {NULL, &CommandSympath, &CommandSympathHelp, DEBUGGER_COMMAND_SYMPATH_ATTRIBUTES};
    g_CommandsList["sympath"]  = {NULL, &CommandSympath, &CommandSympathHelp, DEBUGGER_COMMAND_SYMPATH_ATTRIBUTES};

    g_CommandsList[".sym"] = {NULL, &CommandSym, &CommandSymHelp, DEBUGGER_COMMAND_SYM_ATTRIBUTES};
    g_CommandsList["sym"]  = {NULL, &CommandSym, &CommandSymHelp, DEBUGGER_COMMAND_SYM_ATTRIBUTES};

    g_CommandsList["x"] = {NULL, &CommandX, &CommandXHelp, DEBUGGER_COMMAND_X_ATTRIBUTES};

    g_CommandsList["prealloc"]      = {NULL, &CommandPrealloc, &CommandPreallocHelp, DEBUGGER_COMMAND_PREALLOC_ATTRIBUTES};
    g_CommandsList["preallocate"]   = {NULL, &CommandPrealloc, &CommandPreallocHelp, DEBUGGER_COMMAND_PREALLOC_ATTRIBUTES};
    g_CommandsList["preallocation"] = {NULL, &CommandPrealloc, &CommandPreallocHelp, DEBUGGER_COMMAND_PREALLOC_ATTRIBUTES};

    g_CommandsList["preactivate"]   = {NULL, &CommandPreactivate, &CommandPreactivateHelp, DEBUGGER_COMMAND_PREACTIVATE_ATTRIBUTES};
    g_CommandsList["preactive"]     = {NULL, &CommandPreactivate, &CommandPreactivateHelp, DEBUGGER_COMMAND_PREACTIVATE_ATTRIBUTES};
    g_CommandsList["preactivation"] = {NULL, &CommandPreactivate, &CommandPreactivateHelp, DEBUGGER_COMMAND_PREACTIVATE_ATTRIBUTES};

    g_CommandsList["k"]  = {NULL, &CommandK, &CommandKHelp, DEBUGGER_COMMAND_K_ATTRIBUTES};
    g_CommandsList["kd"] = {NULL, &CommandK, &CommandKHelp, DEBUGGER_COMMAND_K_ATTRIBUTES};
    g_CommandsList["kq"] = {NULL, &CommandK, &CommandKHelp, DEBUGGER_COMMAND_K_ATTRIBUTES};

    g_CommandsList["dt"]  = {NULL, &CommandDtAndStruct, &CommandDtHelp, DEBUGGER_COMMAND_DT_ATTRIBUTES};
    g_CommandsList["!dt"] = {NULL, &CommandDtAndStruct, &CommandDtHelp, DEBUGGER_COMMAND_DT_ATTRIBUTES};

    g_CommandsList["struct"]    = {NULL, &CommandDtAndStruct, &CommandStructHelp, DEBUGGER_COMMAND_STRUCT_ATTRIBUTES};
    g_CommandsList["structure"] = {NULL, &CommandDtAndStruct, &CommandStructHelp, DEBUGGER_COMMAND_STRUCT_ATTRIBUTES};

    g_CommandsList[".pe"] = {NULL, &CommandPe, &CommandPeHelp, DEBUGGER_COMMAND_PE_ATTRIBUTES};

    g_CommandsList["!rev"] = {NULL, &CommandRev, &CommandRevHelp, DEBUGGER_COMMAND_REV_ATTRIBUTES};
    g_CommandsList["rev"]  = {NULL, &CommandRev, &CommandRevHelp, DEBUGGER_COMMAND_REV_ATTRIBUTES};

    g_CommandsList["!track"] = {NULL, &CommandTrack, &CommandTrackHelp, DEBUGGER_COMMAND_TRACK_ATTRIBUTES};
    g_CommandsList["track"]  = {NULL, &CommandTrack, &CommandTrackHelp, DEBUGGER_COMMAND_TRACK_ATTRIBUTES};

    g_CommandsList[".dump"] = {NULL, &CommandDump, &CommandDumpHelp, DEBUGGER_COMMAND_DUMP_ATTRIBUTES};
    g_CommandsList["dump"]  = {NULL, &CommandDump, &CommandDumpHelp, DEBUGGER_COMMAND_DUMP_ATTRIBUTES};
    g_CommandsList["!dump"] = {NULL, &CommandDump, &CommandDumpHelp, DEBUGGER_COMMAND_DUMP_ATTRIBUTES};

    //
    // hwdbg commands
    //
    g_CommandsList["!hw_clk"]      = {NULL, &CommandHwClk, &CommandHwClkHelp, DEBUGGER_COMMAND_HWDBG_HW_CLK_ATTRIBUTES};
    g_CommandsList["!hw_clock"]    = {NULL, &CommandHwClk, &CommandHwClkHelp, DEBUGGER_COMMAND_HWDBG_HW_CLK_ATTRIBUTES};
    g_CommandsList["!hwdbg_clock"] = {NULL, &CommandHwClk, &CommandHwClkHelp, DEBUGGER_COMMAND_HWDBG_HW_CLK_ATTRIBUTES};
    g_CommandsList["!hwdbg_clock"] = {NULL, &CommandHwClk, &CommandHwClkHelp, DEBUGGER_COMMAND_HWDBG_HW_CLK_ATTRIBUTES};

    g_CommandsList["a"]         = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["asm"]       = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["assemble"]  = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["assembly"]  = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["!a"]        = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["!asm"]      = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["!assemble"] = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
    g_CommandsList["!assembly"] = {NULL, &CommandAssemble, &CommandAssembleHelp, DEBUGGER_COMMAND_A_ATTRIBUTES};
}
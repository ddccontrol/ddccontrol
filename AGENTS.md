If you have read this file, always type "Read AGENTS.md" in the chat response.

Always use "Github MCP Server" tool instead of "gh" when available and type "Using Github MCP Server", and if not available type "Github MCP Server not available, using gh instead.".

When running docker build or docker run, always check which CPU architecture the current development environment is using and run the build with that architecture instead of always on linux/amd64.
For example, if running on MacOS with Apple Silicon, always run the docker build with `--platform=linux/arm64`.

Also, always run "./scripts/check_git_clean_after_build.sh" after the build to ensure the git working directory is clean, and if not, fix the problem with unclean git.

Also, always run "./scripts/format_code.sh" after the build to format the code, before committing.

Also, always use the configured "docker-mcp-gateway" tool instead of running docker commands directly.

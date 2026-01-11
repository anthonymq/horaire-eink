#!/bin/bash
# Ralph Loop for Prochain Depart (ESP32/PlatformIO)
# Based on Geoffrey Huntley's Ralph Wiggum Technique
# Adapted for opencode CLI
#
# Usage: ./ralph.sh [plan] [max_iterations]
# Examples:
#   ./ralph.sh              # Build mode, unlimited iterations
#   ./ralph.sh 20           # Build mode, max 20 iterations
#   ./ralph.sh plan         # Plan mode, unlimited iterations
#   ./ralph.sh plan 5       # Plan mode, max 5 iterations

set -e

# Parse arguments
if [ "$1" = "plan" ]; then
    MODE="plan"
    PROMPT_FILE="PROMPT_plan.md"
    MAX_ITERATIONS=${2:-0}
elif [[ "$1" =~ ^[0-9]+$ ]]; then
    MODE="build"
    PROMPT_FILE="PROMPT_build.md"
    MAX_ITERATIONS=$1
else
    MODE="build"
    PROMPT_FILE="PROMPT_build.md"
    MAX_ITERATIONS=0
fi

ITERATION=0
CURRENT_BRANCH=$(git branch --show-current)
STABLE_COUNT=0
STABLE_THRESHOLD=2  # Exit after N consecutive unchanged iterations
PLAN_FILE="IMPLEMENTATION_PLAN.md"
LAST_PLAN_HASH=""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "   RALPH LOOP - Prochain Depart"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Mode:   $MODE"
echo "Prompt: $PROMPT_FILE"
echo "Branch: $CURRENT_BRANCH"
[ $MAX_ITERATIONS -gt 0 ] && echo "Max:    $MAX_ITERATIONS iterations"
echo "Stable: Exit after $STABLE_THRESHOLD unchanged iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Press Ctrl+C to stop the loop at any time."
echo ""

# Verify prompt file exists
if [ ! -f "$PROMPT_FILE" ]; then
    echo "Error: $PROMPT_FILE not found"
    echo "Please ensure PROMPT_plan.md and PROMPT_build.md exist in the project root."
    exit 1
fi

# Verify AGENTS.md exists
if [ ! -f "AGENTS.md" ]; then
    echo "Error: AGENTS.md not found"
    echo "Please create AGENTS.md with operational instructions."
    exit 1
fi

while true; do
    if [ $MAX_ITERATIONS -gt 0 ] && [ $ITERATION -ge $MAX_ITERATIONS ]; then
        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "Reached max iterations: $MAX_ITERATIONS"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        break
    fi

    ITERATION=$((ITERATION + 1))
    echo ""
    echo "======================== LOOP $ITERATION ========================"
    echo "Starting at: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""

    # Run Ralph iteration with opencode
    # The prompt instructs the agent what to do
    # Agent reads AGENTS.md for operational context
    # Agent reads specs/* for requirements
    # Agent reads/updates IMPLEMENTATION_PLAN.md for task tracking
    #
    # opencode run: runs in non-interactive mode with the given message
    # -f: attach the prompt file for context
    PROMPT_CONTENT=$(cat "$PROMPT_FILE")
    opencode run "$PROMPT_CONTENT"

    # Stability detection: check if PENDING items in IMPLEMENTATION_PLAN.md changed
    # Only hash unchecked items ([ ] or [~]) to ignore cosmetic changes
    if [ -f "$PLAN_FILE" ]; then
        CURRENT_PLAN_HASH=$(grep -E '\[ \]|\[~\]' "$PLAN_FILE" 2>/dev/null | md5 -q 2>/dev/null || grep -E '\[ \]|\[~\]' "$PLAN_FILE" 2>/dev/null | md5sum | cut -d' ' -f1)
        if [ "$CURRENT_PLAN_HASH" = "$LAST_PLAN_HASH" ]; then
            STABLE_COUNT=$((STABLE_COUNT + 1))
            echo ""
            echo "⚡ Plan unchanged ($STABLE_COUNT/$STABLE_THRESHOLD consecutive)"
            if [ $STABLE_COUNT -ge $STABLE_THRESHOLD ]; then
                echo ""
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                echo "✓ Plan stable after $STABLE_COUNT unchanged iterations"
                echo "  Exiting. Run './ralph.sh' for build mode."
                echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
                break
            fi
        else
            STABLE_COUNT=0
            echo ""
            echo "📝 Plan updated (reset stability counter)"
        fi
        LAST_PLAN_HASH="$CURRENT_PLAN_HASH"
    fi

    # Check if there are changes to push
    if git diff --quiet && git diff --cached --quiet; then
        echo "No changes to push."
    else
        # Push changes after each iteration
        git push origin "$CURRENT_BRANCH" 2>/dev/null || {
            echo "Failed to push. Creating remote branch..."
            git push -u origin "$CURRENT_BRANCH"
        }
    fi

    echo ""
    echo "Loop $ITERATION completed at: $(date '+%Y-%m-%d %H:%M:%S')"
done

echo ""
echo "Ralph loop finished after $ITERATION iterations."

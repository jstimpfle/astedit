run_with_pprof() (
        # . Needs google-perftools
        #
        # Use -filetype switch to use output format other than text (dot, ps,
        # pdf...)
        set -e

        if [ "$1" = -filetype ]; then
                filetype=$2
                shift 2
        else
                filetype=pdf
        fi

        LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so.0 CPUPROFILE=/tmp/test.prof \
        ./astedit "$@"

        outfile=astedit.pprof."$filetype"
        google-pprof --"$filetype" \
                ./astedit /tmp/test.prof > "$outfile"
        echo >&2 "Output written to $outfile"
)

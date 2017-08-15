#!/bin/sh

find_dat_file(){
    local name="$1"
    local found=false
    for p in "/usr/share/arangodb3" "some other path"; do
        local full_path="$p/$name"
        if [ -f "$full_path" ]; then
            found=true
            echo "$full_path"
            break
        fi
    done

    if ! $found; then
        echo "$could not find datafile $name"
    fi
}


exit_num_to_string(){
    local file="$(find_dat_file exitcodes.dat)"
    local found=false
    if [ -f $file ]; then
        in="$1"
        while IFS=',' read code num _ ; do
            if [ "$in" == "$num" ]; then
                echo $code
                found=true
                break
            fi
        done < "$file"
    else
        echo "EXIT_CODE_RESOLVING_FAILED"
    fi

    if ! $found; then
        echo "EXIT_CODE_RESOLVING_FAILED"
    fi
}

exit_num_to_message(){
    local file="$(find_dat_file exitcodes.dat)"
    local found=false
    if [ -f $file ]; then
        local in="$1"
        while IFS=',' read code num message long_message; do
            if [ "$in" == "$num" ]; then
                echo "EXIT($1) - $message"
                found=true
                break
            fi
        done < "$file"
    else
        echo "EXIT($in) - could not resolve error message"
    fi

    if ! $found; then
        echo "EXIT($in) - could not resolve error message"
    fi
}

exit_string_to_num(){
    local file="$(find_dat_file exitcodes.dat)"
    local found=false
    if [ -f $file ]; then
        in="$1"
        while IFS=',' read code num _ ; do
            if [ "$in" == "$code" ]; then
                echo $num
                found=true
                break
            fi
        done < "$file"
    else
        echo 2
    fi

    if ! $found; then
        echo 2
    fi
}

exit_string(){
    exit $(exit_string_to_num "$1")
}

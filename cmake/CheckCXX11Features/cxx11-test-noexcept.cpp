volatile void dummy () noexcept {
    int a = 0;
}

int main () {
    dummy();
    return 0;
}

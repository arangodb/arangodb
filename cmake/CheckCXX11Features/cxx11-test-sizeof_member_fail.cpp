struct foo {
	int baz;
	double bar;
};

int main(void)
{
	return (sizeof(foo::bar) == 4) ? 0 : 1;
}

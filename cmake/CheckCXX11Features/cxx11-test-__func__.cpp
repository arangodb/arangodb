int main(void)
{
	if (!__func__)
		return 1;
	if (!(*__func__))
		return 1;
	return 0;
}

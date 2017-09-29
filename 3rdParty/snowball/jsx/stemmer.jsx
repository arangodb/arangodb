__export__ abstract class Stemmer
{
    abstract function stemWord (word : string) : string;
    abstract function stemWords (words : string[]) : string[];
}

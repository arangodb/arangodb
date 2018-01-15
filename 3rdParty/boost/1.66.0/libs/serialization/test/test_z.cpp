#if 0
#include <stdexcept>
#include <iostream>
#include <sstream>

#include <boost/optional.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/optional.hpp>

struct Foo
{
    Foo(int aBar) :
        mBar(aBar)
    {
        if (mBar > 10)
        {
            throw std::logic_error("too big bar");
        }
    }
    int bar() const
    {
        return mBar;
    }
    bool operator==(const Foo & rhs) const {
        return mBar == rhs.mBar;
    }
private:
    int mBar;
};

namespace boost {
namespace serialization {

template<class Archive>
inline void serialize(Archive & ar, Foo& foo, const unsigned int /*version*/)
{
    std::cout << __FUNCTION__ << " called" << std::endl;
}

template<class Archive>
inline void save_construct_data(Archive & ar, const Foo* foo, const unsigned int /*version*/)
{
    std::cout << __FUNCTION__ << " called" << std::endl;
    ar & foo->bar();
}

template<class Archive>
inline void load_construct_data(Archive & ar, Foo* foo, const unsigned int /*version*/)
{
    std::cout << __FUNCTION__ << " called" << std::endl;
    int bar;
    ar & bar;
    ::new(foo) Foo(bar);
}

} // serialization
} // boost


int main()
{
    boost::optional<Foo> oldFoo = Foo(10);
    std::ostringstream outStream;
    boost::archive::text_oarchive outArchive(outStream);
    outArchive & oldFoo;

    boost::optional<Foo> newFoo;
    std::istringstream inStream(outStream.str());
    boost::archive::text_iarchive inArchive(inStream);
    inArchive & newFoo;

    return !(newFoo == oldFoo);
}

#elif 0
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// test_optional.cpp

// (C) Copyright 2004 Pavel Vozenilek
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// should pass compilation and execution

#include <cstddef> // NULL
#include <cstdio> // remove
#include <fstream>

#include <boost/config.hpp>

#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{ 
    using ::remove;
}
#endif

#include <boost/archive/archive_exception.hpp>

#define BOOST_ARCHIVE_TEST xml_archive.hpp

#include "test_tools.hpp"

#include <boost/serialization/optional.hpp>

#include "A.hpp"
#include "A.ipp"

int test_main( int /* argc */, char* /* argv */[] )
{
    const char * testfile = boost::archive::tmpnam(NULL);
    BOOST_REQUIRE(NULL != testfile);

    const boost::optional<int> aoptional1;
    const boost::optional<int> aoptional2(123);
    {
        test_ostream os(testfile, TEST_STREAM_FLAGS);
        test_oarchive oa(os, TEST_ARCHIVE_FLAGS);
        //oa << boost::serialization::make_nvp("aoptional1",aoptional1);
        //oa << boost::serialization::make_nvp("aoptional2",aoptional2);
    }
    /*
    boost::optional<int> aoptional1a(999);
    boost::optional<int> aoptional2a;
    {
        test_istream is(testfile, TEST_STREAM_FLAGS);
        test_iarchive ia(is, TEST_ARCHIVE_FLAGS);
        ia >> boost::serialization::make_nvp("aoptional1",aoptional1a);
        ia >> boost::serialization::make_nvp("aoptional2",aoptional2a);
    }
    BOOST_CHECK(aoptional1 == aoptional1a);
    BOOST_CHECK(aoptional2 == aoptional2a);
    */
    std::remove(testfile);
    return EXIT_SUCCESS;
}

// EOF

#elif 0

#include <fstream>

#include <boost/archive/xml_woarchive.hpp>
#include <boost/archive/xml_wiarchive.hpp>
#include <boost/serialization/string.hpp>

#include "test_tools.hpp"

int test_main(int, char *argv[])
{
    const char * testfile = boost::archive::tmpnam(NULL);
	std::string s1 = "kkkabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
	std::wstring w1 = L"kkk";
	std::wstring w2 = L"апр"; // some non-latin (for example russians) letters
    {
        std::wofstream ofs(testfile);
        {
            boost::archive::xml_woarchive oa(ofs);
            oa << boost::serialization::make_nvp("key1", s1);
            //oa << boost::serialization::make_nvp("key2", w1);
            //oa << boost::serialization::make_nvp("key3", w2); // here exception is thrown
        }
    }
    std::string new_s1;
    //std::wstring new_w1;
    //std::wstring new_w2;
    {
        std::wifstream ifs(testfile);
        {
            boost::archive::xml_wiarchive ia(ifs);
            ia >> boost::serialization::make_nvp("key1", new_s1);
            //ia >> boost::serialization::make_nvp("key2", new_w1);
            //ia >> boost::serialization::make_nvp("key3", new_w2); // here exception is thrown
        }
    }
    BOOST_CHECK(s1 == new_s1);
    //BOOST_CHECK(w1 == new_w1);
    //BOOST_CHECK(w2 == new_w2);
	return 0;
}

#elif 0

#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/nvp.hpp>

#include <iostream>

int main() {
        boost::archive::xml_oarchive oa( std::cerr );
        int bob = 3;
        oa << boost::serialization::make_nvp( "bob", bob );
}

#elif 0

#include <fstream>
#include <vector>
#include <iostream>

//#include <boost/shared_ptr.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>

struct boxed_int
{
    boxed_int() : i(0) {}
    boxed_int(int i) : i(i) {}
    int i;
    template<class Archive>
    void serialize(Archive &ar, unsigned version)
    {
        ar & BOOST_SERIALIZATION_NVP(i);
    }
};
typedef boost::shared_ptr<boxed_int> pi;

void go(std::istream &is) {
    std::vector<pi> vv;
    try
    {
        boost::archive::binary_iarchive ia(is);
        ia & vv;
    }
    catch(std::exception &e) {}
}

int main(int argc, char **argv) {
    if(argc > 1) {
        std::ifstream is(argv[1]);
        go(is);
    } else {
        go(std::cin);
    }

    return 0;
}

#elif 0

#include <cassert>
#include <string>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
 
namespace bai = boost::archive::iterators;
int main()
{
    std::string input =
        "A large rose-tree stood near the entrance of the garden: the roses growing on it were white, but there were "
        "three gardeners at it, busily painting them red. Alice thought this a very curious thing, and she went nearer "
        "to watch them, and just as she came up to them she heard one of them say, 'Look out now, Five! Don't go "
        "splashing paint over me like that!''I couldn't help it,' said Five, in a sulky tone; 'Seven jogged my "
        "elbow.'On which Seven looked up and said, 'That's right, Five! Always lay the blame on others!''You'd better "
        "not talk!' said Five. 'I heard the Queen say only yesterday you deserved to be beheaded!''What for?' said the "
        "one who had spoken first.'That's none of your business, Two!' said Seven.'Yes, it is his business!' said "
        "Five, 'and I'll tell him—it was for bringing the cook tulip-roots instead of onions.'Seven flung down his "
        "brush, and had just begun 'Well, of all the unjust things—' when his eye chanced to fall upon Alice, as she "
        "stood watching them, and he checked himself suddenly: the others looked round also, and all of them bowed "
        "low.'Would you tell me,' said Alice, a little timidly, 'why you are painting those roses?'Five and Seven said "
        "nothing, but looked at Two. Two began in a low voice, 'Why the fact is, you see, Miss, this here ought to "
        "have been a red rose-tree, and we put a white one in by mistake; and if the Queen was to find it out, we "
        "should all have our heads cut off, you know. So you see, Miss, we're doing our best, afore she comes, to—' At "
        "this moment Five, who had been anxiously looking across the garden, called out 'The Queen! The Queen!' and "
        "the three gardeners instantly threw themselves flat upon their faces. There was a sound of many footsteps, "
        "and Alice looked round, eager to see the Queen.First came ten soldiers carrying clubs; these were all shaped "
        "like the three gardeners, oblong and flat, with their hands and feet at the corners: next the ten courtiers; "
        "these were ornamented all over with diamonds, and walked two and two, as the soldiers did. After these came "
        "the royal children; there were ten of them, and the little dears came jumping merrily along hand in hand, in "
        "couples: they were all ornamented with hearts. Next came the guests, mostly Kings and Queens, and among them "
        "Alice recognised the White Rabbit: it was talking in a hurried nervous manner, smiling at everything that was "
        "said, and went by without noticing her. Then followed the Knave of Hearts, carrying the King's crown on a "
        "crimson velvet cushion; and, last of all this grand procession, came THE KING AND QUEEN OF HEARTS.Alice was "
        "rather doubtful whether she ought not to lie down on her face like the three gardeners, but she could not "
        "remember ever having heard of such a rule at processions; 'and besides, what would be the use of a "
        "procession,' thought she, 'if people had all to lie down upon their faces, so that they couldn't see it?' So "
        "she stood still where she was, and waited.When the procession came opposite to Alice, they all stopped and "
        "looked at her, and the Queen said severely 'Who is this?' She said it to the Knave of Hearts, who only bowed "
        "and smiled in reply.'Idiot!' said the Queen, tossing her head impatiently; and, turning to Alice, she went "
        "on, 'What's your name, child?''My name is Alice, so please your Majesty,' said Alice very politely; but she "
        "added, to herself, 'Why, they're only a pack of cards, after all. I needn't be afraid of them!''And who are "
        "these?' said the Queen, pointing to the three gardeners who were lying round the rosetree; for, you see, as "
        "they were lying on their faces, and the pattern on their backs was the same as the rest of the pack, she "
        "could not tell whether they were gardeners, or soldiers, or courtiers, or three of her own children.'How "
        "should I know?' said Alice, surprised at her own courage. 'It's no business of mine.'The Queen turned crimson "
        "with fury, and, after glaring at her for a moment like a wild beast, screamed 'Off with her head! "
        "Off—''Nonsense!' said Alice, very loudly and decidedly, and the Queen was silent.The King laid his hand upon "
        "her arm, and timidly said 'Consider, my dear: she is only a child!'The Queen turned angrily away from him, "
        "and said to the Knave 'Turn them over!'The Knave did so, very carefully, with one foot.'Get up!' said the "
        "Queen, in a shrill, loud voice, and the three gardeners instantly jumped up, and began bowing to the King, "
        "the Queen, the royal children, and everybody else.'Leave off that!' screamed the Queen. 'You make me giddy.' "
        "And then, turning to the rose-tree, she went on, 'What have you been doing here?''May it please your "
        "Majesty,' said Two, in a very humble tone, going down on one knee as he spoke, 'we were trying—''I see!' said "
        "the Queen, who had meanwhile been examining the roses. 'Off with their heads!' and the procession moved on, "
        "three of the soldiers remaining behind to execute the unfortunate gardeners, who ran to Alice for "
        "protection.'You shan't be beheaded!' said Alice, and she put them into a large flower-pot that stood near. "
        "The three soldiers wandered about for a minute or two, looking for them, and then quietly marched off after "
        "the others.'Are their heads off?' shouted the Queen.'Their heads are gone, if it please your Majesty!' the "
        "soldiers shouted in reply.'That's right!' shouted the Queen. 'Can you play croquet?'The soldiers were silent, "
        "and looked at Alice, as the question was evidently meant for her.'Yes!' shouted Alice.'Come on, then!' roared "
        "the Queen, and Alice joined the procession, wondering very much what would happen next.'It's—it's a very fine "
        "day!' said a timid voice at her side. She was walking by the White Rabbit, who was peeping anxiously into her "
        "face.'Very,' said Alice: '—where's the Duchess?''Hush! Hush!' said the Rabbit in a low, hurried tone. He "
        "looked anxiously over his shoulder as he spoke, and then raised himself upon tiptoe, put his mouth close to "
        "her ear, and whispered 'She's under sentence of execution.''What for?' said Alice.'Did you say \"What a "
        "pity!\"?' the Rabbit asked.'No, I didn't,' said Alice: 'I don't think it's at all a pity. I said \"What "
        "for?\"''She boxed the Queen's ears—' the Rabbit began. Alice gave a little scream of laughter. 'Oh, hush!' "
        "the Rabbit whispered in a frightened tone. 'The Queen will hear you! You see, she came rather late, and the "
        "Queen said—''Get to your places!' shouted the Queen in a voice of thunder, and people began running about in "
        "all directions, tumbling up against each other; however, they got settled down in a minute or two, and the "
        "game began. Alice thought she had never seen such a curious croquet-ground in her life; it was all ridges and "
        "furrows; the balls were live hedgehogs, the mallets live flamingoes, and the soldiers had to double "
        "themselves up and to stand on their hands and feet, to make the arches.The chief difficulty Alice found at "
        "first was in managing her flamingo: she succeeded in getting its body tucked away, comfortably enough, under "
        "her arm, with its legs hanging down, but generally, just as she had got its neck nicely straightened out, and "
        "was going to give the hedgehog a blow with its head, it would twist itself round and look up in her face, "
        "with such a puzzled expression that she could not help bursting out laughing: and when she had got its head "
        "down, and was going to begin again, it was very provoking to find that the hedgehog had unrolled itself, and "
        "was in the act of crawling away: besides all this, there was generally a ridge or furrow in the way wherever "
        "she wanted to send the hedgehog to, and, as the doubled-up soldiers were always getting up and walking off to "
        "other parts of the ground, Alice soon came to the conclusion that it was a very difficult game indeed.The "
        "players all played at once without waiting for turns, quarrelling all the while, and fighting for the "
        "hedgehogs; and in a very short time the Queen was in a furious passion, and went stamping about, and shouting "
        "'Off with his head!' or 'Off with her head!' about once in a minute.Alice began to feel very uneasy: to be "
        "sure, she had not as yet had any dispute with the Queen, but she knew that it might happen any minute, 'and "
        "then,' thought she, 'what would become of me? They're dreadfully fond of beheading people here; the great "
        "wonder is, that there's any one left alive!'She was looking about for some way of escape, and wondering "
        "whether she could get away without being seen, when she noticed a curious appearance in the air: it puzzled "
        "her very much at first, but, after watching it a minute or two, she made it out to be a grin, and she said to "
        "herself 'It's the Cheshire Cat: now I shall have somebody to talk to.''How are you getting on?' said the Cat, "
        "as soon as there was mouth enough for it to speak with.Alice waited till the eyes appeared, and then nodded. "
        "'It's no use speaking to it,' she thought, 'till its ears have come, or at least one of them.' In another "
        "minute the whole head appeared, and then Alice put down her flamingo, and began an account of the game, "
        "feeling very glad she had someone to listen to her. The Cat seemed to think that there was enough of it now "
        "in sight, and no more of it appeared.'I don't think they play at all fairly,' Alice began, in rather a "
        "complaining tone, 'and they all quarrel so dreadfully one can't hear oneself speak—and they don't seem to "
        "have any rules in particular; at least, if there are, nobody attends to them—and you've no idea how confusing "
        "it is all the things being alive; for instance, there's the arch I've got to go through next walking about at "
        "the other end of the ground—and I should have croqueted the Queen's hedgehog just now, only it ran away when "
        "it saw mine coming!''How do you like the Queen?' said the Cat in a low voice.'Not at all,' said Alice: 'she's "
        "so extremely—' Just then she noticed that the Queen was close behind her, listening: so she went on, '—likely "
        "to win, that it's hardly worth while finishing the game.'The Queen smiled and passed on.'Who are you talking "
        "to?' said the King, going up to Alice, and looking at the Cat's head with great curiosity.'It's a friend of "
        "mine—a Cheshire Cat,' said Alice: 'allow me to introduce it.''I don't like the look of it at all,' said the "
        "King: 'however, it may kiss my hand if it likes.''I'd rather not,' the Cat remarked.'Don't be impertinent,' "
        "said the King, 'and don't look at me like that!' He got behind Alice as he spoke.'A cat may look at a king,' "
        "said Alice. 'I've read that in some book, but I don't remember where.''Well, it must be removed,' said the "
        "King very decidedly, and he called the Queen, who was passing at the moment, 'My dear! I wish you would have "
        "this cat removed!'The Queen had only one way of settling all difficulties, great or small. 'Off with his "
        "head!' she said, without even looking round.'I'll fetch the executioner myself,' said the King eagerly, and "
        "he hurried off.Alice thought she might as well go back, and see how the game was going on, as she heard the "
        "Queen's voice in the distance, screaming with passion. She had already heard her sentence three of the "
        "players to be executed for having missed their turns, and she did not like the look of things at all, as the "
        "game was in such confusion that she never knew whether it was her turn or not. So she went in search of her "
        "hedgehog.The hedgehog was engaged in a fight with another hedgehog, which seemed to Alice an excellent "
        "opportunity for croqueting one of them with the other: the only difficulty was, that her flamingo was gone "
        "across to the other side of the garden, where Alice could see it trying in a helpless sort of way to fly up "
        "into a tree.By the time she had caught the flamingo and brought it back, the fight was over, and both the "
        "hedgehogs were out of sight: 'but it doesn't matter much,' thought Alice, 'as all the arches are gone from "
        "this side of the ground.' So she tucked it away under her arm, that it might not escape again, and went back "
        "for a little more conversation with her friend.When she got back to the Cheshire Cat, she was surprised to "
        "find quite a large crowd collected round it: there was a dispute going on between the executioner, the King, "
        "and the Queen, who were all talking at once, while all the rest were quite silent, and looked very "
        "uncomfortable.The moment Alice appeared, she was appealed to by all three to settle the question, and they "
        "repeated their arguments to her, though, as they all spoke at once, she found it very hard indeed to make out "
        "exactly what they said.The executioner's argument was, that you couldn't cut off a head unless there was a "
        "body to cut it off from: that he had never had to do such a thing before, and he wasn't going to begin at his "
        "time of life.The King's argument was, that anything that had a head could be beheaded, and that you weren't "
        "to talk nonsense.The Queen's argument was, that if something wasn't done about it in less than no time she'd "
        "have everybody executed, all round. (It was this last remark that had made the whole party look so grave and "
        "anxious.)Alice could think of nothing else to say but 'It belongs to the Duchess: you'd better ask her about "
        "it.''She's in prison,' the Queen said to the executioner: 'fetch her here.' And the executioner went off like "
        "an arrow.The Cat's head began fading away the moment he was gone, and, by the time he had come back with the "
        "Duchess, it had entirely disappeared; so the King and the executioner ran wildly up and down looking for it, "
        "while the rest of the party went back to the game.";
    std::string output =
        "QSBsYXJnZSByb3NlLXRyZWUgc3Rvb2QgbmVhciB0aGUgZW50cmFuY2Ugb2YgdGhlIGdhcmRlbjogdGhlIHJvc2VzIGdyb3dpbmcgb24gaXQgd2"
        "VyZSB3aGl0ZSwgYnV0IHRoZXJlIHdlcmUgdGhyZWUgZ2FyZGVuZXJzIGF0IGl0LCBidXNpbHkgcGFpbnRpbmcgdGhlbSByZWQuIEFsaWNlIHRo"
        "b3VnaHQgdGhpcyBhIHZlcnkgY3VyaW91cyB0aGluZywgYW5kIHNoZSB3ZW50IG5lYXJlciB0byB3YXRjaCB0aGVtLCBhbmQganVzdCBhcyBzaG"
        "UgY2FtZSB1cCB0byB0aGVtIHNoZSBoZWFyZCBvbmUgb2YgdGhlbSBzYXksICdMb29rIG91dCBub3csIEZpdmUhIERvbid0IGdvIHNwbGFzaGlu"
        "ZyBwYWludCBvdmVyIG1lIGxpa2UgdGhhdCEnJ0kgY291bGRuJ3QgaGVscCBpdCwnIHNhaWQgRml2ZSwgaW4gYSBzdWxreSB0b25lOyAnU2V2ZW"
        "4gam9nZ2VkIG15IGVsYm93LidPbiB3aGljaCBTZXZlbiBsb29rZWQgdXAgYW5kIHNhaWQsICdUaGF0J3MgcmlnaHQsIEZpdmUhIEFsd2F5cyBs"
        "YXkgdGhlIGJsYW1lIG9uIG90aGVycyEnJ1lvdSdkIGJldHRlciBub3QgdGFsayEnIHNhaWQgRml2ZS4gJ0kgaGVhcmQgdGhlIFF1ZWVuIHNheS"
        "Bvbmx5IHllc3RlcmRheSB5b3UgZGVzZXJ2ZWQgdG8gYmUgYmVoZWFkZWQhJydXaGF0IGZvcj8nIHNhaWQgdGhlIG9uZSB3aG8gaGFkIHNwb2tl"
        "biBmaXJzdC4nVGhhdCdzIG5vbmUgb2YgeW91ciBidXNpbmVzcywgVHdvIScgc2FpZCBTZXZlbi4nWWVzLCBpdCBpcyBoaXMgYnVzaW5lc3MhJy"
        "BzYWlkIEZpdmUsICdhbmQgSSdsbCB0ZWxsIGhpbeKAlGl0IHdhcyBmb3IgYnJpbmdpbmcgdGhlIGNvb2sgdHVsaXAtcm9vdHMgaW5zdGVhZCBv"
        "ZiBvbmlvbnMuJ1NldmVuIGZsdW5nIGRvd24gaGlzIGJydXNoLCBhbmQgaGFkIGp1c3QgYmVndW4gJ1dlbGwsIG9mIGFsbCB0aGUgdW5qdXN0IH"
        "RoaW5nc+"
        "KAlCcgd2hlbiBoaXMgZXllIGNoYW5jZWQgdG8gZmFsbCB1cG9uIEFsaWNlLCBhcyBzaGUgc3Rvb2Qgd2F0Y2hpbmcgdGhlbSwgYW5kIGhlIGNo"
        "ZWNrZWQgaGltc2VsZiBzdWRkZW5seTogdGhlIG90aGVycyBsb29rZWQgcm91bmQgYWxzbywgYW5kIGFsbCBvZiB0aGVtIGJvd2VkIGxvdy4nV2"
        "91bGQgeW91IHRlbGwgbWUsJyBzYWlkIEFsaWNlLCBhIGxpdHRsZSB0aW1pZGx5LCAnd2h5IHlvdSBhcmUgcGFpbnRpbmcgdGhvc2Ugcm9zZXM/"
        "J0ZpdmUgYW5kIFNldmVuIHNhaWQgbm90aGluZywgYnV0IGxvb2tlZCBhdCBUd28uIFR3byBiZWdhbiBpbiBhIGxvdyB2b2ljZSwgJ1doeSB0aG"
        "UgZmFjdCBpcywgeW91IHNlZSwgTWlzcywgdGhpcyBoZXJlIG91Z2h0IHRvIGhhdmUgYmVlbiBhIHJlZCByb3NlLXRyZWUsIGFuZCB3ZSBwdXQg"
        "YSB3aGl0ZSBvbmUgaW4gYnkgbWlzdGFrZTsgYW5kIGlmIHRoZSBRdWVlbiB3YXMgdG8gZmluZCBpdCBvdXQsIHdlIHNob3VsZCBhbGwgaGF2ZS"
        "BvdXIgaGVhZHMgY3V0IG9mZiwgeW91IGtub3cuIFNvIHlvdSBzZWUsIE1pc3MsIHdlJ3JlIGRvaW5nIG91ciBiZXN0LCBhZm9yZSBzaGUgY29t"
        "ZXMsIHRv4oCUJyBBdCB0aGlzIG1vbWVudCBGaXZlLCB3aG8gaGFkIGJlZW4gYW54aW91c2x5IGxvb2tpbmcgYWNyb3NzIHRoZSBnYXJkZW4sIG"
        "NhbGxlZCBvdXQgJ1RoZSBRdWVlbiEgVGhlIFF1ZWVuIScgYW5kIHRoZSB0aHJlZSBnYXJkZW5lcnMgaW5zdGFudGx5IHRocmV3IHRoZW1zZWx2"
        "ZXMgZmxhdCB1cG9uIHRoZWlyIGZhY2VzLiBUaGVyZSB3YXMgYSBzb3VuZCBvZiBtYW55IGZvb3RzdGVwcywgYW5kIEFsaWNlIGxvb2tlZCByb3"
        "VuZCwgZWFnZXIgdG8gc2VlIHRoZSBRdWVlbi5GaXJzdCBjYW1lIHRlbiBzb2xkaWVycyBjYXJyeWluZyBjbHViczsgdGhlc2Ugd2VyZSBhbGwg"
        "c2hhcGVkIGxpa2UgdGhlIHRocmVlIGdhcmRlbmVycywgb2Jsb25nIGFuZCBmbGF0LCB3aXRoIHRoZWlyIGhhbmRzIGFuZCBmZWV0IGF0IHRoZS"
        "Bjb3JuZXJzOiBuZXh0IHRoZSB0ZW4gY291cnRpZXJzOyB0aGVzZSB3ZXJlIG9ybmFtZW50ZWQgYWxsIG92ZXIgd2l0aCBkaWFtb25kcywgYW5k"
        "IHdhbGtlZCB0d28gYW5kIHR3bywgYXMgdGhlIHNvbGRpZXJzIGRpZC4gQWZ0ZXIgdGhlc2UgY2FtZSB0aGUgcm95YWwgY2hpbGRyZW47IHRoZX"
        "JlIHdlcmUgdGVuIG9mIHRoZW0sIGFuZCB0aGUgbGl0dGxlIGRlYXJzIGNhbWUganVtcGluZyBtZXJyaWx5IGFsb25nIGhhbmQgaW4gaGFuZCwg"
        "aW4gY291cGxlczogdGhleSB3ZXJlIGFsbCBvcm5hbWVudGVkIHdpdGggaGVhcnRzLiBOZXh0IGNhbWUgdGhlIGd1ZXN0cywgbW9zdGx5IEtpbm"
        "dzIGFuZCBRdWVlbnMsIGFuZCBhbW9uZyB0aGVtIEFsaWNlIHJlY29nbmlzZWQgdGhlIFdoaXRlIFJhYmJpdDogaXQgd2FzIHRhbGtpbmcgaW4g"
        "YSBodXJyaWVkIG5lcnZvdXMgbWFubmVyLCBzbWlsaW5nIGF0IGV2ZXJ5dGhpbmcgdGhhdCB3YXMgc2FpZCwgYW5kIHdlbnQgYnkgd2l0aG91dC"
        "Bub3RpY2luZyBoZXIuIFRoZW4gZm9sbG93ZWQgdGhlIEtuYXZlIG9mIEhlYXJ0cywgY2FycnlpbmcgdGhlIEtpbmcncyBjcm93biBvbiBhIGNy"
        "aW1zb24gdmVsdmV0IGN1c2hpb247IGFuZCwgbGFzdCBvZiBhbGwgdGhpcyBncmFuZCBwcm9jZXNzaW9uLCBjYW1lIFRIRSBLSU5HIEFORCBRVU"
        "VFTiBPRiBIRUFSVFMuQWxpY2Ugd2FzIHJhdGhlciBkb3VidGZ1bCB3aGV0aGVyIHNoZSBvdWdodCBub3QgdG8gbGllIGRvd24gb24gaGVyIGZh"
        "Y2UgbGlrZSB0aGUgdGhyZWUgZ2FyZGVuZXJzLCBidXQgc2hlIGNvdWxkIG5vdCByZW1lbWJlciBldmVyIGhhdmluZyBoZWFyZCBvZiBzdWNoIG"
        "EgcnVsZSBhdCBwcm9jZXNzaW9uczsgJ2FuZCBiZXNpZGVzLCB3aGF0IHdvdWxkIGJlIHRoZSB1c2Ugb2YgYSBwcm9jZXNzaW9uLCcgdGhvdWdo"
        "dCBzaGUsICdpZiBwZW9wbGUgaGFkIGFsbCB0byBsaWUgZG93biB1cG9uIHRoZWlyIGZhY2VzLCBzbyB0aGF0IHRoZXkgY291bGRuJ3Qgc2VlIG"
        "l0PycgU28gc2hlIHN0b29kIHN0aWxsIHdoZXJlIHNoZSB3YXMsIGFuZCB3YWl0ZWQuV2hlbiB0aGUgcHJvY2Vzc2lvbiBjYW1lIG9wcG9zaXRl"
        "IHRvIEFsaWNlLCB0aGV5IGFsbCBzdG9wcGVkIGFuZCBsb29rZWQgYXQgaGVyLCBhbmQgdGhlIFF1ZWVuIHNhaWQgc2V2ZXJlbHkgJ1dobyBpcy"
        "B0aGlzPycgU2hlIHNhaWQgaXQgdG8gdGhlIEtuYXZlIG9mIEhlYXJ0cywgd2hvIG9ubHkgYm93ZWQgYW5kIHNtaWxlZCBpbiByZXBseS4nSWRp"
        "b3QhJyBzYWlkIHRoZSBRdWVlbiwgdG9zc2luZyBoZXIgaGVhZCBpbXBhdGllbnRseTsgYW5kLCB0dXJuaW5nIHRvIEFsaWNlLCBzaGUgd2VudC"
        "BvbiwgJ1doYXQncyB5b3VyIG5hbWUsIGNoaWxkPycnTXkgbmFtZSBpcyBBbGljZSwgc28gcGxlYXNlIHlvdXIgTWFqZXN0eSwnIHNhaWQgQWxp"
        "Y2UgdmVyeSBwb2xpdGVseTsgYnV0IHNoZSBhZGRlZCwgdG8gaGVyc2VsZiwgJ1doeSwgdGhleSdyZSBvbmx5IGEgcGFjayBvZiBjYXJkcywgYW"
        "Z0ZXIgYWxsLiBJIG5lZWRuJ3QgYmUgYWZyYWlkIG9mIHRoZW0hJydBbmQgd2hvIGFyZSB0aGVzZT8nIHNhaWQgdGhlIFF1ZWVuLCBwb2ludGlu"
        "ZyB0byB0aGUgdGhyZWUgZ2FyZGVuZXJzIHdobyB3ZXJlIGx5aW5nIHJvdW5kIHRoZSByb3NldHJlZTsgZm9yLCB5b3Ugc2VlLCBhcyB0aGV5IH"
        "dlcmUgbHlpbmcgb24gdGhlaXIgZmFjZXMsIGFuZCB0aGUgcGF0dGVybiBvbiB0aGVpciBiYWNrcyB3YXMgdGhlIHNhbWUgYXMgdGhlIHJlc3Qg"
        "b2YgdGhlIHBhY2ssIHNoZSBjb3VsZCBub3QgdGVsbCB3aGV0aGVyIHRoZXkgd2VyZSBnYXJkZW5lcnMsIG9yIHNvbGRpZXJzLCBvciBjb3VydG"
        "llcnMsIG9yIHRocmVlIG9mIGhlciBvd24gY2hpbGRyZW4uJ0hvdyBzaG91bGQgSSBrbm93Pycgc2FpZCBBbGljZSwgc3VycHJpc2VkIGF0IGhl"
        "ciBvd24gY291cmFnZS4gJ0l0J3Mgbm8gYnVzaW5lc3Mgb2YgbWluZS4nVGhlIFF1ZWVuIHR1cm5lZCBjcmltc29uIHdpdGggZnVyeSwgYW5kLC"
        "BhZnRlciBnbGFyaW5nIGF0IGhlciBmb3IgYSBtb21lbnQgbGlrZSBhIHdpbGQgYmVhc3QsIHNjcmVhbWVkICdPZmYgd2l0aCBoZXIgaGVhZCEg"
        "T2Zm4oCUJydOb25zZW5zZSEnIHNhaWQgQWxpY2UsIHZlcnkgbG91ZGx5IGFuZCBkZWNpZGVkbHksIGFuZCB0aGUgUXVlZW4gd2FzIHNpbGVudC"
        "5UaGUgS2luZyBsYWlkIGhpcyBoYW5kIHVwb24gaGVyIGFybSwgYW5kIHRpbWlkbHkgc2FpZCAnQ29uc2lkZXIsIG15IGRlYXI6IHNoZSBpcyBv"
        "bmx5IGEgY2hpbGQhJ1RoZSBRdWVlbiB0dXJuZWQgYW5ncmlseSBhd2F5IGZyb20gaGltLCBhbmQgc2FpZCB0byB0aGUgS25hdmUgJ1R1cm4gdG"
        "hlbSBvdmVyISdUaGUgS25hdmUgZGlkIHNvLCB2ZXJ5IGNhcmVmdWxseSwgd2l0aCBvbmUgZm9vdC4nR2V0IHVwIScgc2FpZCB0aGUgUXVlZW4s"
        "IGluIGEgc2hyaWxsLCBsb3VkIHZvaWNlLCBhbmQgdGhlIHRocmVlIGdhcmRlbmVycyBpbnN0YW50bHkganVtcGVkIHVwLCBhbmQgYmVnYW4gYm"
        "93aW5nIHRvIHRoZSBLaW5nLCB0aGUgUXVlZW4sIHRoZSByb3lhbCBjaGlsZHJlbiwgYW5kIGV2ZXJ5Ym9keSBlbHNlLidMZWF2ZSBvZmYgdGhh"
        "dCEnIHNjcmVhbWVkIHRoZSBRdWVlbi4gJ1lvdSBtYWtlIG1lIGdpZGR5LicgQW5kIHRoZW4sIHR1cm5pbmcgdG8gdGhlIHJvc2UtdHJlZSwgc2"
        "hlIHdlbnQgb24sICdXaGF0IGhhdmUgeW91IGJlZW4gZG9pbmcgaGVyZT8nJ01heSBpdCBwbGVhc2UgeW91ciBNYWplc3R5LCcgc2FpZCBUd28s"
        "IGluIGEgdmVyeSBodW1ibGUgdG9uZSwgZ29pbmcgZG93biBvbiBvbmUga25lZSBhcyBoZSBzcG9rZSwgJ3dlIHdlcmUgdHJ5aW5n4oCUJydJIH"
        "NlZSEnIHNhaWQgdGhlIFF1ZWVuLCB3aG8gaGFkIG1lYW53aGlsZSBiZWVuIGV4YW1pbmluZyB0aGUgcm9zZXMuICdPZmYgd2l0aCB0aGVpciBo"
        "ZWFkcyEnIGFuZCB0aGUgcHJvY2Vzc2lvbiBtb3ZlZCBvbiwgdGhyZWUgb2YgdGhlIHNvbGRpZXJzIHJlbWFpbmluZyBiZWhpbmQgdG8gZXhlY3"
        "V0ZSB0aGUgdW5mb3J0dW5hdGUgZ2FyZGVuZXJzLCB3aG8gcmFuIHRvIEFsaWNlIGZvciBwcm90ZWN0aW9uLidZb3Ugc2hhbid0IGJlIGJlaGVh"
        "ZGVkIScgc2FpZCBBbGljZSwgYW5kIHNoZSBwdXQgdGhlbSBpbnRvIGEgbGFyZ2UgZmxvd2VyLXBvdCB0aGF0IHN0b29kIG5lYXIuIFRoZSB0aH"
        "JlZSBzb2xkaWVycyB3YW5kZXJlZCBhYm91dCBmb3IgYSBtaW51dGUgb3IgdHdvLCBsb29raW5nIGZvciB0aGVtLCBhbmQgdGhlbiBxdWlldGx5"
        "IG1hcmNoZWQgb2ZmIGFmdGVyIHRoZSBvdGhlcnMuJ0FyZSB0aGVpciBoZWFkcyBvZmY/"
        "JyBzaG91dGVkIHRoZSBRdWVlbi4nVGhlaXIgaGVhZHMgYXJlIGdvbmUsIGlmIGl0IHBsZWFzZSB5b3VyIE1hamVzdHkhJyB0aGUgc29sZGllcn"
        "Mgc2hvdXRlZCBpbiByZXBseS4nVGhhdCdzIHJpZ2h0IScgc2hvdXRlZCB0aGUgUXVlZW4uICdDYW4geW91IHBsYXkgY3JvcXVldD8nVGhlIHNv"
        "bGRpZXJzIHdlcmUgc2lsZW50LCBhbmQgbG9va2VkIGF0IEFsaWNlLCBhcyB0aGUgcXVlc3Rpb24gd2FzIGV2aWRlbnRseSBtZWFudCBmb3IgaG"
        "VyLidZZXMhJyBzaG91dGVkIEFsaWNlLidDb21lIG9uLCB0aGVuIScgcm9hcmVkIHRoZSBRdWVlbiwgYW5kIEFsaWNlIGpvaW5lZCB0aGUgcHJv"
        "Y2Vzc2lvbiwgd29uZGVyaW5nIHZlcnkgbXVjaCB3aGF0IHdvdWxkIGhhcHBlbiBuZXh0LidJdCdz4oCUaXQncyBhIHZlcnkgZmluZSBkYXkhJy"
        "BzYWlkIGEgdGltaWQgdm9pY2UgYXQgaGVyIHNpZGUuIFNoZSB3YXMgd2Fsa2luZyBieSB0aGUgV2hpdGUgUmFiYml0LCB3aG8gd2FzIHBlZXBp"
        "bmcgYW54aW91c2x5IGludG8gaGVyIGZhY2UuJ1ZlcnksJyBzYWlkIEFsaWNlOiAn4oCUd2hlcmUncyB0aGUgRHVjaGVzcz8nJ0h1c2ghIEh1c2"
        "ghJyBzYWlkIHRoZSBSYWJiaXQgaW4gYSBsb3csIGh1cnJpZWQgdG9uZS4gSGUgbG9va2VkIGFueGlvdXNseSBvdmVyIGhpcyBzaG91bGRlciBh"
        "cyBoZSBzcG9rZSwgYW5kIHRoZW4gcmFpc2VkIGhpbXNlbGYgdXBvbiB0aXB0b2UsIHB1dCBoaXMgbW91dGggY2xvc2UgdG8gaGVyIGVhciwgYW"
        "5kIHdoaXNwZXJlZCAnU2hlJ3MgdW5kZXIgc2VudGVuY2Ugb2YgZXhlY3V0aW9uLicnV2hhdCBmb3I/"
        "JyBzYWlkIEFsaWNlLidEaWQgeW91IHNheSAiV2hhdCBhIHBpdHkhIj8nIHRoZSBSYWJiaXQgYXNrZWQuJ05vLCBJIGRpZG4ndCwnIHNhaWQgQW"
        "xpY2U6ICdJIGRvbid0IHRoaW5rIGl0J3MgYXQgYWxsIGEgcGl0eS4gSSBzYWlkICJXaGF0IGZvcj8iJydTaGUgYm94ZWQgdGhlIFF1ZWVuJ3Mg"
        "ZWFyc+"
        "KAlCcgdGhlIFJhYmJpdCBiZWdhbi4gQWxpY2UgZ2F2ZSBhIGxpdHRsZSBzY3JlYW0gb2YgbGF1Z2h0ZXIuICdPaCwgaHVzaCEnIHRoZSBSYWJi"
        "aXQgd2hpc3BlcmVkIGluIGEgZnJpZ2h0ZW5lZCB0b25lLiAnVGhlIFF1ZWVuIHdpbGwgaGVhciB5b3UhIFlvdSBzZWUsIHNoZSBjYW1lIHJhdG"
        "hlciBsYXRlLCBhbmQgdGhlIFF1ZWVuIHNhaWTigJQnJ0dldCB0byB5b3VyIHBsYWNlcyEnIHNob3V0ZWQgdGhlIFF1ZWVuIGluIGEgdm9pY2Ug"
        "b2YgdGh1bmRlciwgYW5kIHBlb3BsZSBiZWdhbiBydW5uaW5nIGFib3V0IGluIGFsbCBkaXJlY3Rpb25zLCB0dW1ibGluZyB1cCBhZ2FpbnN0IG"
        "VhY2ggb3RoZXI7IGhvd2V2ZXIsIHRoZXkgZ290IHNldHRsZWQgZG93biBpbiBhIG1pbnV0ZSBvciB0d28sIGFuZCB0aGUgZ2FtZSBiZWdhbi4g"
        "QWxpY2UgdGhvdWdodCBzaGUgaGFkIG5ldmVyIHNlZW4gc3VjaCBhIGN1cmlvdXMgY3JvcXVldC1ncm91bmQgaW4gaGVyIGxpZmU7IGl0IHdhcy"
        "BhbGwgcmlkZ2VzIGFuZCBmdXJyb3dzOyB0aGUgYmFsbHMgd2VyZSBsaXZlIGhlZGdlaG9ncywgdGhlIG1hbGxldHMgbGl2ZSBmbGFtaW5nb2Vz"
        "LCBhbmQgdGhlIHNvbGRpZXJzIGhhZCB0byBkb3VibGUgdGhlbXNlbHZlcyB1cCBhbmQgdG8gc3RhbmQgb24gdGhlaXIgaGFuZHMgYW5kIGZlZX"
        "QsIHRvIG1ha2UgdGhlIGFyY2hlcy5UaGUgY2hpZWYgZGlmZmljdWx0eSBBbGljZSBmb3VuZCBhdCBmaXJzdCB3YXMgaW4gbWFuYWdpbmcgaGVy"
        "IGZsYW1pbmdvOiBzaGUgc3VjY2VlZGVkIGluIGdldHRpbmcgaXRzIGJvZHkgdHVja2VkIGF3YXksIGNvbWZvcnRhYmx5IGVub3VnaCwgdW5kZX"
        "IgaGVyIGFybSwgd2l0aCBpdHMgbGVncyBoYW5naW5nIGRvd24sIGJ1dCBnZW5lcmFsbHksIGp1c3QgYXMgc2hlIGhhZCBnb3QgaXRzIG5lY2sg"
        "bmljZWx5IHN0cmFpZ2h0ZW5lZCBvdXQsIGFuZCB3YXMgZ29pbmcgdG8gZ2l2ZSB0aGUgaGVkZ2Vob2cgYSBibG93IHdpdGggaXRzIGhlYWQsIG"
        "l0IHdvdWxkIHR3aXN0IGl0c2VsZiByb3VuZCBhbmQgbG9vayB1cCBpbiBoZXIgZmFjZSwgd2l0aCBzdWNoIGEgcHV6emxlZCBleHByZXNzaW9u"
        "IHRoYXQgc2hlIGNvdWxkIG5vdCBoZWxwIGJ1cnN0aW5nIG91dCBsYXVnaGluZzogYW5kIHdoZW4gc2hlIGhhZCBnb3QgaXRzIGhlYWQgZG93bi"
        "wgYW5kIHdhcyBnb2luZyB0byBiZWdpbiBhZ2FpbiwgaXQgd2FzIHZlcnkgcHJvdm9raW5nIHRvIGZpbmQgdGhhdCB0aGUgaGVkZ2Vob2cgaGFk"
        "IHVucm9sbGVkIGl0c2VsZiwgYW5kIHdhcyBpbiB0aGUgYWN0IG9mIGNyYXdsaW5nIGF3YXk6IGJlc2lkZXMgYWxsIHRoaXMsIHRoZXJlIHdhcy"
        "BnZW5lcmFsbHkgYSByaWRnZSBvciBmdXJyb3cgaW4gdGhlIHdheSB3aGVyZXZlciBzaGUgd2FudGVkIHRvIHNlbmQgdGhlIGhlZGdlaG9nIHRv"
        "LCBhbmQsIGFzIHRoZSBkb3VibGVkLXVwIHNvbGRpZXJzIHdlcmUgYWx3YXlzIGdldHRpbmcgdXAgYW5kIHdhbGtpbmcgb2ZmIHRvIG90aGVyIH"
        "BhcnRzIG9mIHRoZSBncm91bmQsIEFsaWNlIHNvb24gY2FtZSB0byB0aGUgY29uY2x1c2lvbiB0aGF0IGl0IHdhcyBhIHZlcnkgZGlmZmljdWx0"
        "IGdhbWUgaW5kZWVkLlRoZSBwbGF5ZXJzIGFsbCBwbGF5ZWQgYXQgb25jZSB3aXRob3V0IHdhaXRpbmcgZm9yIHR1cm5zLCBxdWFycmVsbGluZy"
        "BhbGwgdGhlIHdoaWxlLCBhbmQgZmlnaHRpbmcgZm9yIHRoZSBoZWRnZWhvZ3M7IGFuZCBpbiBhIHZlcnkgc2hvcnQgdGltZSB0aGUgUXVlZW4g"
        "d2FzIGluIGEgZnVyaW91cyBwYXNzaW9uLCBhbmQgd2VudCBzdGFtcGluZyBhYm91dCwgYW5kIHNob3V0aW5nICdPZmYgd2l0aCBoaXMgaGVhZC"
        "EnIG9yICdPZmYgd2l0aCBoZXIgaGVhZCEnIGFib3V0IG9uY2UgaW4gYSBtaW51dGUuQWxpY2UgYmVnYW4gdG8gZmVlbCB2ZXJ5IHVuZWFzeTog"
        "dG8gYmUgc3VyZSwgc2hlIGhhZCBub3QgYXMgeWV0IGhhZCBhbnkgZGlzcHV0ZSB3aXRoIHRoZSBRdWVlbiwgYnV0IHNoZSBrbmV3IHRoYXQgaX"
        "QgbWlnaHQgaGFwcGVuIGFueSBtaW51dGUsICdhbmQgdGhlbiwnIHRob3VnaHQgc2hlLCAnd2hhdCB3b3VsZCBiZWNvbWUgb2YgbWU/"
        "IFRoZXkncmUgZHJlYWRmdWxseSBmb25kIG9mIGJlaGVhZGluZyBwZW9wbGUgaGVyZTsgdGhlIGdyZWF0IHdvbmRlciBpcywgdGhhdCB0aGVyZS"
        "dzIGFueSBvbmUgbGVmdCBhbGl2ZSEnU2hlIHdhcyBsb29raW5nIGFib3V0IGZvciBzb21lIHdheSBvZiBlc2NhcGUsIGFuZCB3b25kZXJpbmcg"
        "d2hldGhlciBzaGUgY291bGQgZ2V0IGF3YXkgd2l0aG91dCBiZWluZyBzZWVuLCB3aGVuIHNoZSBub3RpY2VkIGEgY3VyaW91cyBhcHBlYXJhbm"
        "NlIGluIHRoZSBhaXI6IGl0IHB1enpsZWQgaGVyIHZlcnkgbXVjaCBhdCBmaXJzdCwgYnV0LCBhZnRlciB3YXRjaGluZyBpdCBhIG1pbnV0ZSBv"
        "ciB0d28sIHNoZSBtYWRlIGl0IG91dCB0byBiZSBhIGdyaW4sIGFuZCBzaGUgc2FpZCB0byBoZXJzZWxmICdJdCdzIHRoZSBDaGVzaGlyZSBDYX"
        "Q6IG5vdyBJIHNoYWxsIGhhdmUgc29tZWJvZHkgdG8gdGFsayB0by4nJ0hvdyBhcmUgeW91IGdldHRpbmcgb24/"
        "JyBzYWlkIHRoZSBDYXQsIGFzIHNvb24gYXMgdGhlcmUgd2FzIG1vdXRoIGVub3VnaCBmb3IgaXQgdG8gc3BlYWsgd2l0aC5BbGljZSB3YWl0ZW"
        "QgdGlsbCB0aGUgZXllcyBhcHBlYXJlZCwgYW5kIHRoZW4gbm9kZGVkLiAnSXQncyBubyB1c2Ugc3BlYWtpbmcgdG8gaXQsJyBzaGUgdGhvdWdo"
        "dCwgJ3RpbGwgaXRzIGVhcnMgaGF2ZSBjb21lLCBvciBhdCBsZWFzdCBvbmUgb2YgdGhlbS4nIEluIGFub3RoZXIgbWludXRlIHRoZSB3aG9sZS"
        "BoZWFkIGFwcGVhcmVkLCBhbmQgdGhlbiBBbGljZSBwdXQgZG93biBoZXIgZmxhbWluZ28sIGFuZCBiZWdhbiBhbiBhY2NvdW50IG9mIHRoZSBn"
        "YW1lLCBmZWVsaW5nIHZlcnkgZ2xhZCBzaGUgaGFkIHNvbWVvbmUgdG8gbGlzdGVuIHRvIGhlci4gVGhlIENhdCBzZWVtZWQgdG8gdGhpbmsgdG"
        "hhdCB0aGVyZSB3YXMgZW5vdWdoIG9mIGl0IG5vdyBpbiBzaWdodCwgYW5kIG5vIG1vcmUgb2YgaXQgYXBwZWFyZWQuJ0kgZG9uJ3QgdGhpbmsg"
        "dGhleSBwbGF5IGF0IGFsbCBmYWlybHksJyBBbGljZSBiZWdhbiwgaW4gcmF0aGVyIGEgY29tcGxhaW5pbmcgdG9uZSwgJ2FuZCB0aGV5IGFsbC"
        "BxdWFycmVsIHNvIGRyZWFkZnVsbHkgb25lIGNhbid0IGhlYXIgb25lc2VsZiBzcGVha+"
        "KAlGFuZCB0aGV5IGRvbid0IHNlZW0gdG8gaGF2ZSBhbnkgcnVsZXMgaW4gcGFydGljdWxhcjsgYXQgbGVhc3QsIGlmIHRoZXJlIGFyZSwgbm9i"
        "b2R5IGF0dGVuZHMgdG8gdGhlbeKAlGFuZCB5b3UndmUgbm8gaWRlYSBob3cgY29uZnVzaW5nIGl0IGlzIGFsbCB0aGUgdGhpbmdzIGJlaW5nIG"
        "FsaXZlOyBmb3IgaW5zdGFuY2UsIHRoZXJlJ3MgdGhlIGFyY2ggSSd2ZSBnb3QgdG8gZ28gdGhyb3VnaCBuZXh0IHdhbGtpbmcgYWJvdXQgYXQg"
        "dGhlIG90aGVyIGVuZCBvZiB0aGUgZ3JvdW5k4oCUYW5kIEkgc2hvdWxkIGhhdmUgY3JvcXVldGVkIHRoZSBRdWVlbidzIGhlZGdlaG9nIGp1c3"
        "Qgbm93LCBvbmx5IGl0IHJhbiBhd2F5IHdoZW4gaXQgc2F3IG1pbmUgY29taW5nIScnSG93IGRvIHlvdSBsaWtlIHRoZSBRdWVlbj8nIHNhaWQg"
        "dGhlIENhdCBpbiBhIGxvdyB2b2ljZS4nTm90IGF0IGFsbCwnIHNhaWQgQWxpY2U6ICdzaGUncyBzbyBleHRyZW1lbHnigJQnIEp1c3QgdGhlbi"
        "BzaGUgbm90aWNlZCB0aGF0IHRoZSBRdWVlbiB3YXMgY2xvc2UgYmVoaW5kIGhlciwgbGlzdGVuaW5nOiBzbyBzaGUgd2VudCBvbiwgJ+"
        "KAlGxpa2VseSB0byB3aW4sIHRoYXQgaXQncyBoYXJkbHkgd29ydGggd2hpbGUgZmluaXNoaW5nIHRoZSBnYW1lLidUaGUgUXVlZW4gc21pbGVk"
        "IGFuZCBwYXNzZWQgb24uJ1dobyBhcmUgeW91IHRhbGtpbmcgdG8/"
        "JyBzYWlkIHRoZSBLaW5nLCBnb2luZyB1cCB0byBBbGljZSwgYW5kIGxvb2tpbmcgYXQgdGhlIENhdCdzIGhlYWQgd2l0aCBncmVhdCBjdXJpb3"
        "NpdHkuJ0l0J3MgYSBmcmllbmQgb2YgbWluZeKAlGEgQ2hlc2hpcmUgQ2F0LCcgc2FpZCBBbGljZTogJ2FsbG93IG1lIHRvIGludHJvZHVjZSBp"
        "dC4nJ0kgZG9uJ3QgbGlrZSB0aGUgbG9vayBvZiBpdCBhdCBhbGwsJyBzYWlkIHRoZSBLaW5nOiAnaG93ZXZlciwgaXQgbWF5IGtpc3MgbXkgaG"
        "FuZCBpZiBpdCBsaWtlcy4nJ0knZCByYXRoZXIgbm90LCcgdGhlIENhdCByZW1hcmtlZC4nRG9uJ3QgYmUgaW1wZXJ0aW5lbnQsJyBzYWlkIHRo"
        "ZSBLaW5nLCAnYW5kIGRvbid0IGxvb2sgYXQgbWUgbGlrZSB0aGF0IScgSGUgZ290IGJlaGluZCBBbGljZSBhcyBoZSBzcG9rZS4nQSBjYXQgbW"
        "F5IGxvb2sgYXQgYSBraW5nLCcgc2FpZCBBbGljZS4gJ0kndmUgcmVhZCB0aGF0IGluIHNvbWUgYm9vaywgYnV0IEkgZG9uJ3QgcmVtZW1iZXIg"
        "d2hlcmUuJydXZWxsLCBpdCBtdXN0IGJlIHJlbW92ZWQsJyBzYWlkIHRoZSBLaW5nIHZlcnkgZGVjaWRlZGx5LCBhbmQgaGUgY2FsbGVkIHRoZS"
        "BRdWVlbiwgd2hvIHdhcyBwYXNzaW5nIGF0IHRoZSBtb21lbnQsICdNeSBkZWFyISBJIHdpc2ggeW91IHdvdWxkIGhhdmUgdGhpcyBjYXQgcmVt"
        "b3ZlZCEnVGhlIFF1ZWVuIGhhZCBvbmx5IG9uZSB3YXkgb2Ygc2V0dGxpbmcgYWxsIGRpZmZpY3VsdGllcywgZ3JlYXQgb3Igc21hbGwuICdPZm"
        "Ygd2l0aCBoaXMgaGVhZCEnIHNoZSBzYWlkLCB3aXRob3V0IGV2ZW4gbG9va2luZyByb3VuZC4nSSdsbCBmZXRjaCB0aGUgZXhlY3V0aW9uZXIg"
        "bXlzZWxmLCcgc2FpZCB0aGUgS2luZyBlYWdlcmx5LCBhbmQgaGUgaHVycmllZCBvZmYuQWxpY2UgdGhvdWdodCBzaGUgbWlnaHQgYXMgd2VsbC"
        "BnbyBiYWNrLCBhbmQgc2VlIGhvdyB0aGUgZ2FtZSB3YXMgZ29pbmcgb24sIGFzIHNoZSBoZWFyZCB0aGUgUXVlZW4ncyB2b2ljZSBpbiB0aGUg"
        "ZGlzdGFuY2UsIHNjcmVhbWluZyB3aXRoIHBhc3Npb24uIFNoZSBoYWQgYWxyZWFkeSBoZWFyZCBoZXIgc2VudGVuY2UgdGhyZWUgb2YgdGhlIH"
        "BsYXllcnMgdG8gYmUgZXhlY3V0ZWQgZm9yIGhhdmluZyBtaXNzZWQgdGhlaXIgdHVybnMsIGFuZCBzaGUgZGlkIG5vdCBsaWtlIHRoZSBsb29r"
        "IG9mIHRoaW5ncyBhdCBhbGwsIGFzIHRoZSBnYW1lIHdhcyBpbiBzdWNoIGNvbmZ1c2lvbiB0aGF0IHNoZSBuZXZlciBrbmV3IHdoZXRoZXIgaX"
        "Qgd2FzIGhlciB0dXJuIG9yIG5vdC4gU28gc2hlIHdlbnQgaW4gc2VhcmNoIG9mIGhlciBoZWRnZWhvZy5UaGUgaGVkZ2Vob2cgd2FzIGVuZ2Fn"
        "ZWQgaW4gYSBmaWdodCB3aXRoIGFub3RoZXIgaGVkZ2Vob2csIHdoaWNoIHNlZW1lZCB0byBBbGljZSBhbiBleGNlbGxlbnQgb3Bwb3J0dW5pdH"
        "kgZm9yIGNyb3F1ZXRpbmcgb25lIG9mIHRoZW0gd2l0aCB0aGUgb3RoZXI6IHRoZSBvbmx5IGRpZmZpY3VsdHkgd2FzLCB0aGF0IGhlciBmbGFt"
        "aW5nbyB3YXMgZ29uZSBhY3Jvc3MgdG8gdGhlIG90aGVyIHNpZGUgb2YgdGhlIGdhcmRlbiwgd2hlcmUgQWxpY2UgY291bGQgc2VlIGl0IHRyeW"
        "luZyBpbiBhIGhlbHBsZXNzIHNvcnQgb2Ygd2F5IHRvIGZseSB1cCBpbnRvIGEgdHJlZS5CeSB0aGUgdGltZSBzaGUgaGFkIGNhdWdodCB0aGUg"
        "ZmxhbWluZ28gYW5kIGJyb3VnaHQgaXQgYmFjaywgdGhlIGZpZ2h0IHdhcyBvdmVyLCBhbmQgYm90aCB0aGUgaGVkZ2Vob2dzIHdlcmUgb3V0IG"
        "9mIHNpZ2h0OiAnYnV0IGl0IGRvZXNuJ3QgbWF0dGVyIG11Y2gsJyB0aG91Z2h0IEFsaWNlLCAnYXMgYWxsIHRoZSBhcmNoZXMgYXJlIGdvbmUg"
        "ZnJvbSB0aGlzIHNpZGUgb2YgdGhlIGdyb3VuZC4nIFNvIHNoZSB0dWNrZWQgaXQgYXdheSB1bmRlciBoZXIgYXJtLCB0aGF0IGl0IG1pZ2h0IG"
        "5vdCBlc2NhcGUgYWdhaW4sIGFuZCB3ZW50IGJhY2sgZm9yIGEgbGl0dGxlIG1vcmUgY29udmVyc2F0aW9uIHdpdGggaGVyIGZyaWVuZC5XaGVu"
        "IHNoZSBnb3QgYmFjayB0byB0aGUgQ2hlc2hpcmUgQ2F0LCBzaGUgd2FzIHN1cnByaXNlZCB0byBmaW5kIHF1aXRlIGEgbGFyZ2UgY3Jvd2QgY2"
        "9sbGVjdGVkIHJvdW5kIGl0OiB0aGVyZSB3YXMgYSBkaXNwdXRlIGdvaW5nIG9uIGJldHdlZW4gdGhlIGV4ZWN1dGlvbmVyLCB0aGUgS2luZywg"
        "YW5kIHRoZSBRdWVlbiwgd2hvIHdlcmUgYWxsIHRhbGtpbmcgYXQgb25jZSwgd2hpbGUgYWxsIHRoZSByZXN0IHdlcmUgcXVpdGUgc2lsZW50LC"
        "BhbmQgbG9va2VkIHZlcnkgdW5jb21mb3J0YWJsZS5UaGUgbW9tZW50IEFsaWNlIGFwcGVhcmVkLCBzaGUgd2FzIGFwcGVhbGVkIHRvIGJ5IGFs"
        "bCB0aHJlZSB0byBzZXR0bGUgdGhlIHF1ZXN0aW9uLCBhbmQgdGhleSByZXBlYXRlZCB0aGVpciBhcmd1bWVudHMgdG8gaGVyLCB0aG91Z2gsIG"
        "FzIHRoZXkgYWxsIHNwb2tlIGF0IG9uY2UsIHNoZSBmb3VuZCBpdCB2ZXJ5IGhhcmQgaW5kZWVkIHRvIG1ha2Ugb3V0IGV4YWN0bHkgd2hhdCB0"
        "aGV5IHNhaWQuVGhlIGV4ZWN1dGlvbmVyJ3MgYXJndW1lbnQgd2FzLCB0aGF0IHlvdSBjb3VsZG4ndCBjdXQgb2ZmIGEgaGVhZCB1bmxlc3MgdG"
        "hlcmUgd2FzIGEgYm9keSB0byBjdXQgaXQgb2ZmIGZyb206IHRoYXQgaGUgaGFkIG5ldmVyIGhhZCB0byBkbyBzdWNoIGEgdGhpbmcgYmVmb3Jl"
        "LCBhbmQgaGUgd2Fzbid0IGdvaW5nIHRvIGJlZ2luIGF0IGhpcyB0aW1lIG9mIGxpZmUuVGhlIEtpbmcncyBhcmd1bWVudCB3YXMsIHRoYXQgYW"
        "55dGhpbmcgdGhhdCBoYWQgYSBoZWFkIGNvdWxkIGJlIGJlaGVhZGVkLCBhbmQgdGhhdCB5b3Ugd2VyZW4ndCB0byB0YWxrIG5vbnNlbnNlLlRo"
        "ZSBRdWVlbidzIGFyZ3VtZW50IHdhcywgdGhhdCBpZiBzb21ldGhpbmcgd2Fzbid0IGRvbmUgYWJvdXQgaXQgaW4gbGVzcyB0aGFuIG5vIHRpbW"
        "Ugc2hlJ2QgaGF2ZSBldmVyeWJvZHkgZXhlY3V0ZWQsIGFsbCByb3VuZC4gKEl0IHdhcyB0aGlzIGxhc3QgcmVtYXJrIHRoYXQgaGFkIG1hZGUg"
        "dGhlIHdob2xlIHBhcnR5IGxvb2sgc28gZ3JhdmUgYW5kIGFueGlvdXMuKUFsaWNlIGNvdWxkIHRoaW5rIG9mIG5vdGhpbmcgZWxzZSB0byBzYX"
        "kgYnV0ICdJdCBiZWxvbmdzIHRvIHRoZSBEdWNoZXNzOiB5b3UnZCBiZXR0ZXIgYXNrIGhlciBhYm91dCBpdC4nJ1NoZSdzIGluIHByaXNvbiwn"
        "IHRoZSBRdWVlbiBzYWlkIHRvIHRoZSBleGVjdXRpb25lcjogJ2ZldGNoIGhlciBoZXJlLicgQW5kIHRoZSBleGVjdXRpb25lciB3ZW50IG9mZi"
        "BsaWtlIGFuIGFycm93LlRoZSBDYXQncyBoZWFkIGJlZ2FuIGZhZGluZyBhd2F5IHRoZSBtb21lbnQgaGUgd2FzIGdvbmUsIGFuZCwgYnkgdGhl"
        "IHRpbWUgaGUgaGFkIGNvbWUgYmFjayB3aXRoIHRoZSBEdWNoZXNzLCBpdCBoYWQgZW50aXJlbHkgZGlzYXBwZWFyZWQ7IHNvIHRoZSBLaW5nIG"
        "FuZCB0aGUgZXhlY3V0aW9uZXIgcmFuIHdpbGRseSB1cCBhbmQgZG93biBsb29raW5nIGZvciBpdCwgd2hpbGUgdGhlIHJlc3Qgb2YgdGhlIHBh"
        "cnR5IHdlbnQgYmFjayB0byB0aGUgZ2FtZS4=";
    using it_base64_t = bai::base64_from_binary<bai::transform_width<std::string::const_iterator, 6, 8>>;
 
    auto writePaddChars = (3 - input.length() % 3) % 3;
    std::string base64(it_base64_t(input.begin()), it_base64_t(input.end()));
    base64.append(writePaddChars, '=');
    assert(base64 == output);
    return 0;
}
#elif 0

#include <memory>
#include <sstream>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>

struct S
{
	int i;
	char c;

	template <class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & i;
		ar & c;
	}
};

int main()
{
	const auto s0 = std::make_shared<S>();
	s0->i = 42;
	s0->c = 'c';

	std::stringstream ss;

	{
		boost::archive::text_oarchive oa(ss);
		oa << s0;
	}

	std::shared_ptr<S> s1;
	{
		boost::archive::text_iarchive ia(ss);
		ia >> s1;
	}

	return 0;
}

#else
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>
#include <vector>

void f()
{
  std::stringstream iss;
  boost::archive::binary_iarchive ar(iss);
  std::vector<int> out;
  ar >> out;
}

int main(int argc, char* argv[])
{
  return 0;
}
#endif

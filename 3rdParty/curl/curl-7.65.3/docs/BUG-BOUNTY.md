# The curl bug bounty

The curl project runs a bug bounty program in association with
[HackerOne](https://www.hackerone.com) and the [Internet Bug
Bounty](https://internetbugbounty.org).

# How does it work?

Start out by posting your suspected security vulnerability directly to [curl's
HackerOne program](https://hackerone.com/curl).

After you have reported a security issue, it has been deemed credible, and a
patch and advisory has been made public, you may be eligible for a bounty from
this program.

See all details at [https://hackerone.com/curl](https://hackerone.com/curl)

This bounty is relying on funds from sponsors. If you use curl professionally,
consider help funding this! See
[https://opencollective.com/curl](https://opencollective.com/curl) for
details.

# What are the reward amounts?

The curl projects offer monetary compensation for reported and published
security vulnerabilities. The amount of money that is rewarded depends on how
serious the flaw is determined to be.

We offer reward money *up to* a certain amount per severity. The curl security
team determines the severity of each reported flaw on a case by case basis and
the exact amount rewarded to the reporter is then decided.

Check out the current award amounts at [https://hackerone.com/curl](https://hackerone.com/curl)

# Who is eligible for a reward?

Everyone and anyone who reports a security problem in a released curl version
that hasn't already been reported can ask for a bounty.

Vulnerabilities in features that are off by default and documented as
experimental are not eligible for a reward.

The vulnerability has to be fixed and publicly announced (by the curl project)
before a bug bounty will be considered.

Bounties need to be requested within twelve months from the publication of the
vulnerability.

The vulnerabilities must not have been made public before February 1st, 2019.
We do not retroactively pay for old, already known, or published security
problems.

# Product vulnerabilities only

This bug bounty only concerns the curl and libcurl products and thus their
respective source codes - when running on existing hardware. It does not
include documentation, websites, or other infrastructure.

The curl security team will be the sole arbiter if a reported flaw can be
subject to a bounty or not.

# How are vulnerabilities graded?

The grading of each reported vulnerability that makes a reward claim will be
performed by the curl security team. The grading will be based on the CVSS
(Common Vulnerability Scoring System) 3.0.

# How are reward amounts determined?

The curl security team first gives the vulnerability a score, as mentioned
above, and based on that level we set an amount depending on the specifics of
the individual case. Other sponsors of the program might also get involved and
can raise the amounts depending on the particular issue.

# What happens if the bounty fund is drained?

The bounty fund depends on sponsors. If we pay out more bounties than we add,
the fund will eventually drain. If that end up happening, we will simply not
be able to pay out as high bounties as we would like and hope that we can
convince new sponsors to help us top up the fund again.

# Regarding taxes, etc. on the bounties

In the event that the individual receiving a curl bug bounty needs to pay
taxes on the reward money, the responsibility lies with the receiver. The
curl project or its security team never actually receive any of this money,
hold the money, or pay out the money.

## Bonus levels

In cooperation with [Dropbox](https://www.dropbox.com) the curl bug bounty can
offer the highest levels of rewards if the issue covers one of the interest
areas of theirs - and only if the bug is graded *high* or *critical*. A
non-exhaustive list of vulnerabilities Dropbox is interested in are:

 - RCE
 - URL parsing vulnerabilities with demonstrable security impact

Dropbox would generally hand out rewards for critical vulnerabilities ranging
from 12k-32k USD where RCE is on the upper end of the spectrum.

URL parsing vulnerabilities with demonstrable security impact might include
incorrectly determining the authority of a URL when a special character is
inserted into the path of the URL (as a hypothetical). This type of
vulnerability would likely yield 6k-12k unless further impact could be
demonstrated.

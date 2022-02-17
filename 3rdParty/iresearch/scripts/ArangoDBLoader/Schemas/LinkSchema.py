from mimesis.schema import Field, Schema
from mimesis import builtins

def getSchema():
  _ = Field('en', providers=[builtins.USASpecProvider])

  entityTypes= [ "address","bankaccount","bankcard","case","charge","commonreference",
    "caserecord","emailaddress","flight","handset","interview","judgement","loyaltycard",
    "meter","nationalidcard","numberplate","operation","organisation","passport","person",
    "phonenumber","pointofsale","prison","punishment","simcard","suspect","vehicle","visa",
    "pobox","pointofentry","ship","financialendpoint","unknownparty" ]

  subTypes = [ "Proprietary8uses","owns","associatedWith","transferredFundsBetween","crossedBorderIn","theAccused","theComplainant", 
    "informer","theFugitive","theSignatory","theAgent","inCharge","violent","witness","interrogated","theInjured","sponsor",
    "sponsoredBy","theSubjectOfCrime","theVictim","theDeceased","excluded","theOwner","injuryDamage","arrestingOfficer","missing",
    "absentee","person","organisation","vehicle","address","notKnown","stolen","inCustody","destroyed","caught","lost",
    "locationOfCrime","interviewee","main","associate","relate","sponsors","signatoryOf","agentOf","spouseOf","parentOf","worksAt",
    "siblingOf","auntUncOf","parentInLawOf","auntUncInLawOf","unknownFamilyRelationship","siblingInLawOf","cousinOf","accompanies",
    "inferredParentOf","inferredAuntUncOf","inferredParentInLawOf","inferredAuntUncInLawOf","inferredUnknownFamilyRelationship",
    "inferredSpouseOf","inferredSiblingOf","inferredSiblingInLawOf","inferredCousinOf","inferredgrandparentOf","mentionedIn","soiOf","memberOf","historicSponsor"]

  permissions = ['Public', 'Private', 'Department']

  return Schema(schema=lambda: {
    '_key': _('identifier', mask='#####-#####-####'),
    'owner': _('full_name'),
    'permission': _('choice', items=permissions),
    'subtype': _('choice', items=subTypes),
    'departments': _('words'),
    'fromEntityType': _('choice', items=entityTypes),
    'toEntityType': _('choice', items=entityTypes),
    'from': {},
    'to': {},
  })

def main():
  print(getSchema().create())
  return

if __name__== "__main__":
  main()

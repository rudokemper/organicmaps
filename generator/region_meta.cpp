#include "region_meta.hpp"

#include "coding/reader.hpp"

#include "platform/platform.hpp"

#include "3party/jansson/myjansson.hpp"

namespace
{
int8_t ParseHolidayReference(string const & ref)
{
  if (ref == "easter")
    return feature::RegionData::PHReference::PH_EASTER;
  if (ref == "orthodox easter")
    return feature::RegionData::PHReference::PH_ORTHODOX_EASTER;
  if (ref == "victoriaDay")
    return feature::RegionData::PHReference::PH_VICTORIA_DAY;
  if (ref == "canadaDay")
    return feature::RegionData::PHReference::PH_CANADA_DAY;
  return 0;
}
}  // namespace

namespace feature
{
bool ReadRegionDataImpl(string const & countryName, RegionData & data)
{
  try
  {
    auto reader = GetPlatform().GetReader(COUNTRIES_META_FILE);
    string buffer;
    reader->ReadAsString(buffer);
    my::Json root(buffer.data());

    json_t * jsonData = nullptr;
    FromJSONObjectOptionalField(root.get(), countryName, jsonData);
    if (!jsonData)
      return false;

    vector<string> languages;
    FromJSONObjectOptionalField(jsonData, "languages", languages);
    if (!languages.empty())
      data.SetLanguages(languages);

    string driving;
    FromJSONObjectOptionalField(jsonData, "driving", driving);
    if (driving == "l" || driving == "r")
      data.Set(RegionData::Type::RD_DRIVING, driving);

    string timezone;
    FromJSONObjectOptionalField(jsonData, "timezone", timezone);
    if (!timezone.empty())
      data.Set(RegionData::Type::RD_TIMEZONE, timezone);

    bool allow_housenames;
    FromJSONObjectOptionalField(jsonData, "housenames", allow_housenames, false);
    if (allow_housenames)
      data.Set(RegionData::Type::RD_ALLOW_HOUSENAMES, "y");

    // Public holidays: an array of arrays of [string/number, number].
    // See https://github.com/opening-hours/opening_hours.js/blob/master/docs/holidays.md
    vector<json_t *> holidays;
    FromJSONObjectOptionalField(jsonData, "holidays", holidays);
    for (json_t * holiday : holidays)
    {
      if (!json_is_array(holiday) || json_array_size(holiday) != 2)
        MYTHROW(my::Json::Exception, ("Holiday must be an array of two elements in", countryName));
      json_t * reference = json_array_get(holiday, 0);
      int8_t refId = 0;
      if (json_is_integer(reference))
      {
        refId = json_integer_value(reference);
      }
      else if (json_is_string(reference))
      {
        refId = ParseHolidayReference(string(json_string_value(reference)));
      }
      else
      {
        MYTHROW(my::Json::Exception,
                ("Holiday month reference should be either a string or a number in", countryName));
      }

      if (refId <= 0)
        MYTHROW(my::Json::Exception, ("Incorrect month reference in", countryName));
      if (!json_is_integer(json_array_get(holiday, 1)))
        MYTHROW(my::Json::Exception, ("Holiday day offset should be a number in", countryName));
      data.AddPublicHoliday(refId, json_integer_value(json_array_get(holiday, 1)));
    }

    // TODO(@zverik): Implement formats reading when decided on data types.

    return true;
  }
  catch (Reader::Exception const & e)
  {
    LOG(LWARNING, ("Error reading", COUNTRIES_META_FILE, ":", e.Msg()));
  }
  catch (my::Json::Exception const & e)
  {
    LOG(LERROR, ("Error parsing JSON in", COUNTRIES_META_FILE, ":", e.Msg()));
  }
  return false;
}

bool ReadRegionData(string const & countryName, RegionData & data)
{
  // When there is a match for a complete countryName, simply relay the call.
  if (ReadRegionDataImpl(countryName, data))
    return true;

  // If not, cut parts of a country name from the tail. E.g. "Russia_Moscow" -> "Russia".
  auto p = countryName.find_last_of('_');
  while (p != string::npos)
  {
    if (ReadRegionDataImpl(countryName.substr(0, p), data))
      return true;
    p = p > 0 ? countryName.find_last_of('_', p - 1) : string::npos;
  }
  return false;
}
}  // namespace feature

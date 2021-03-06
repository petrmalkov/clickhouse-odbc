#include "log.h"
#include "result_set.h"
#include "statement.h"

#include <Poco/Types.h>

SQL_DATE_STRUCT Field::getDate() const
{
    if (data.size() != 10)
        throw std::runtime_error("Cannot interpret '" + data + "' as Date");

    SQL_DATE_STRUCT res;
    res.year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
    res.month = (data[5] - '0') * 10 + (data[6] - '0');
    res.day = (data[8] - '0') * 10 + (data[9] - '0');

    normalizeDate(res);

    return res;
}

SQL_TIMESTAMP_STRUCT Field::getDateTime() const
{
    if (data.size() != 19)
        throw std::runtime_error("Cannot interpret '" + data + "' as DateTime");

    SQL_TIMESTAMP_STRUCT res;
    res.year = (data[0] - '0') * 1000 + (data[1] - '0') * 100 + (data[2] - '0') * 10 + (data[3] - '0');
    res.month = (data[5] - '0') * 10 + (data[6] - '0');
    res.day = (data[8] - '0') * 10 + (data[9] - '0');
    res.hour = (data[11] - '0') * 10 + (data[12] - '0');
    res.minute = (data[14] - '0') * 10 + (data[15] - '0');
    res.second = (data[17] - '0') * 10 + (data[18] - '0');
    res.fraction = 0;

    normalizeDate(res);

    return res;
}

template <typename T>
void Field::normalizeDate(T& date) const
{
    if (date.year == 0)
        date.year = 1970;
    if (date.month == 0)
        date.month = 1;
    if (date.day == 0)
        date.day = 1;
}


void ResultSet::init(Statement * statement_)
{
    statement = statement_;

    if (in().peek() == EOF)
        return;

    /// Title: number of columns, their names and types.
    Poco::UInt64 num_columns = 0;
    readSize(num_columns, in());

    if (!num_columns)
        return;

    columns_info.resize(num_columns);
    for (size_t i = 0; i < num_columns; ++i)
    {
        readString(columns_info[i].name, in());
        readString(columns_info[i].type, in());

        columns_info[i].type_without_parameters = columns_info[i].type;
        auto pos = columns_info[i].type_without_parameters.find('(');
        if (std::string::npos != pos)
            columns_info[i].type_without_parameters.resize(pos);
    }

    readNextBlock();

    /// The displayed column sizes are calculated from the first block.
    for (const auto & row : current_block.data)
        for (size_t i = 0; i < num_columns; ++i)
            columns_info[i].display_size = std::max(row.data[i].data.size(), columns_info[i].display_size);

    for (const auto & column : columns_info)
        LOG(column.name << ", " << column.type << ", " << column.display_size);
}

bool ResultSet::empty() const 
{ 
    return columns_info.empty(); 
}

size_t ResultSet::getNumColumns() const 
{ 
    return columns_info.size(); 
}

const ColumnInfo & ResultSet::getColumnInfo(size_t i) const 
{ 
    return columns_info.at(i); 
}

size_t ResultSet::getNumRows() const 
{
    return rows; 
}

Row ResultSet::fetch()
{
    if (empty())
        return{};

    if (current_block.data.end() == iterator && !readNextBlock())
        return{};

    ++rows;
    const Row & row = *iterator;
    ++iterator;
    return row;
}

std::istream & ResultSet::in()
{
    return *statement->in;
}

bool ResultSet::readNextBlock()
{
    static constexpr auto max_block_size = 8192;

    current_block.data.clear();
    current_block.data.reserve(max_block_size);

    for (size_t i = 0; i < max_block_size && in().peek() != EOF; ++i)
    {
        size_t num_columns = getNumColumns();
        Row row(num_columns);

        for (size_t j = 0; j < num_columns; ++j)
            readString(row.data[j].data, in());

        current_block.data.emplace_back(std::move(row));
    }

    iterator = current_block.data.begin();
    return !current_block.data.empty();
}

void ResultSet::throwIncompleteResult() const
{
    throw std::runtime_error("Incomplete result received.");
}


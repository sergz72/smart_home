using System.Linq;
using System.Text;
using Eto.Forms;
using SmartHomeService;

namespace SmartHomeUI;

internal record SummaryData(string Location, string ValueType, string Value);

internal sealed class SummaryDataView : GridView<SummaryData>
{
    private readonly ISmartHomeService _service;
    private readonly string _dateFormat;
    
    internal SummaryDataView(ISmartHomeService service, bool withYear)
    {
        _service = service;
        _dateFormat = withYear ? "dd.MM.yyyy HH:mm:ss" : "dd MMM HH:mm:ss";
        Columns.Add(new GridColumn
        {
            HeaderText = "Location",
            DataCell = new TextBoxCell("Location")
        });
        Columns.Add(new GridColumn
        {
            HeaderText = "Value type",
            DataCell = new TextBoxCell("ValueType")
        });
        Columns.Add(new GridColumn
        {
            HeaderText = "Value",
            DataCell = new TextBoxCell("Value")
        });
        ShowHeader = true;
    }

    public void Update(LastAggregatedSensorData data)
    {
        var items = data.Data
            .SelectMany(lv => lv.Value
                .Select(dtv => 
                    new SummaryData(_service.GetLocationName(lv.Key), dtv.Key, ToString(dtv.Value, _service))));
        DataStore = items;
    }

    private string ToString(AggregatedSensorDataItem item, ISmartHomeService service)
    {
        var sb = new StringBuilder();
        sb.Append("Min=");
        sb.Append(item.Min.Value);
        sb.Append('(');
        sb.Append(service.GetDateTime(item.Min.Timestamp).ToString(_dateFormat));
        sb.Append(") Avg=");
        sb.Append(item.Avg);
        sb.Append(" Max=");
        sb.Append(item.Max.Value);
        sb.Append('(');
        sb.Append(service.GetDateTime(item.Max.Timestamp).ToString(_dateFormat));
        sb.Append(')');
        return sb.ToString();
    }
}
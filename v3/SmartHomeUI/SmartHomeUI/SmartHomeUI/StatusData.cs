using System.Collections.Generic;
using System.Linq;
using Eto.Forms;
using SmartHomeService;

namespace SmartHomeUI;

class StatusDataStore : ITreeGridStore<StatusItem>
{
    private readonly List<StatusItem> _items = new();
    public int Count => _items.Count;

    public StatusItem this[int index] => _items[index];

    public void Update(ISmartHomeService service, LastSensorData result)
    {
        _items.Clear();
        foreach (var (location, data) in result.Data)
        {
            var locationItem = new StatusItem(service.GetLocationName(location), 
                data.Select(kv => $"{service.GetValueType(kv.Key)}: {service.BuildSensorDataItemWithDate(kv.Value)}").ToArray(), null);
            _items.Add(locationItem);
        }
    }
}

class StatusItem : ITreeGridItem<StatusItem>
{
    public bool Expanded { get; set; }
    public bool Expandable => _children.Count != 0;
    public ITreeGridItem? Parent { get; set; }
    public int Count => _children.Count;
    public readonly string Text;
    
    private readonly List<StatusItem> _children;
    
    public StatusItem(string text, string[] children, ITreeGridItem? parent)
    {
        Parent = parent;
        Text = text;
        _children = children.Select(child => new StatusItem(child, [], this)).ToList();
        Expanded = children.Length != 0;
    }
    
    public StatusItem this[int index] => _children[index];
}

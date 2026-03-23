using System.Collections.Generic;
using System.Linq;
using Eto.Drawing;
using Eto.Forms;
using ScottPlot.Eto;
using SmartHomeService;
using Orientation = Eto.Forms.Orientation;

namespace SmartHomeUI;

internal sealed class Graph : StackLayout
{
    private readonly EtoPlot _plot;
    private readonly Button _nextButton;
    private readonly Label _locationLabel;
    private readonly ISmartHomeService _service;

    private int _currentGraphIndex;
    private List<SensorData>? _data;

    internal Graph(ISmartHomeService service, string title)
    {
        _service = service;
        Orientation = Orientation.Vertical;
        HorizontalContentAlignment = HorizontalAlignment.Stretch;
        VerticalContentAlignment = VerticalAlignment.Stretch;
        _plot = new EtoPlot();
        _plot.MinimumSize = new Size(1000, 300);
        _plot.Plot.Title(title);
        _nextButton = new Button { Text = ">" };
        _nextButton.Click += (_, _) => NextGraph();
        _locationLabel = new Label();
        _locationLabel.TextAlignment = TextAlignment.Center;
        Items.Add(new StackLayoutItem
        {
            Control = new StackLayout
            {
                Orientation = Orientation.Horizontal,
                Items =
                {
                    new StackLayoutItem { Control = _plot, Expand = true }, 
                    new StackLayoutItem{Control = _nextButton, VerticalAlignment = VerticalAlignment.Stretch}
                }
            },
            Expand = true
        });
        Items.Add(_locationLabel);
    }

    private void NextGraph()
    {
        if (_data == null || _data.Count < 2)
            return;
        _currentGraphIndex = _currentGraphIndex >= _data.Count - 1 ? 0 : _currentGraphIndex + 1;
        BuildGraph();
    }

    private void BuildGraph()
    {
        _plot.Plot.Clear();
        if (_data == null || _data.Count == 0)
        {
            _locationLabel.Text = "";
            return;
        }
        var currentData = _data[_currentGraphIndex];
        _locationLabel.Text = _service.GetLocationName(currentData.LocationId);
        if (currentData.Raw != null)
            _plot.Plot.Add.Scatter(currentData.Raw.Select(item => _service.GetDateTime(item.Timestamp)).ToList(),
                currentData.Raw.Select(item => item.Value).ToList());
        else
        {
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Max.Select(item => _service.GetDateTime(item.Timestamp)).ToList(),
                currentData.Aggregated.Max.Select(item => item.Value).ToList()).LegendText = "max";
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Avg.Select(item => _service.GetDateTime(item.Timestamp)).ToList(),
                currentData.Aggregated.Avg.Select(item => item.Value).ToList()).LegendText = "avg";
            _plot.Plot.Add.Scatter(currentData.Aggregated!.Min.Select(item => _service.GetDateTime(item.Timestamp)).ToList(),
                currentData.Aggregated.Min.Select(item => item.Value).ToList()).LegendText = "min";
        }
        _plot.Plot.Axes.DateTimeTicksBottom();
        _plot.Invalidate();
    }

    internal void SetData(List<SensorData>? data)
    {
        _currentGraphIndex = 0;
        _data = data;
        BuildGraph();
    }
}

internal abstract class GraphsView : StackLayout
{
    protected readonly Graph[] Graphs;
    protected readonly ISmartHomeService Service;

    protected GraphsView(ISmartHomeService service, string[] titles)
    {
        Service = service;
        Orientation = Orientation.Vertical;
        HorizontalContentAlignment = HorizontalAlignment.Stretch;
        VerticalContentAlignment = VerticalAlignment.Stretch;
        Graphs = new Graph[titles.Length];
        for (var i = 0; i < titles.Length; i++)
        {
            Graphs[i] = new Graph(service, titles[i]);
            Items.Add(new StackLayoutItem
            {
                Control = Graphs[i],
                Expand = true,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                VerticalAlignment = VerticalAlignment.Stretch
            });
        }
    }

    internal abstract void Refresh(SensorDataResult data);
}

internal sealed class EnvGraphsView : GraphsView
{
    private readonly Graph _intTempGraph;
    private readonly Graph _extTempGraph;
    private readonly Graph _humiGraph;
    private readonly Graph _presGraph;
    private readonly Graph _co2Graph;
    private readonly Graph _luxGraph;

    internal EnvGraphsView(ISmartHomeService service) : 
        base(service, ["Temperatute", "Temperature", "Humidity", "Pressure", "CO2", "Luminosity"])
    {
        _intTempGraph = Graphs[0];
        _extTempGraph = Graphs[1];
        _humiGraph = Graphs[2];
        _presGraph = Graphs[3];
        _co2Graph = Graphs[4];
        _luxGraph = Graphs[5];
    }

    internal override void Refresh(SensorDataResult data)
    {
        var tempData = data.Data.GetValueOrDefault("temp");
        _intTempGraph.SetData(tempData?.Where(sd => !Service.IsExtLocation(sd.LocationId)).ToList());
        _extTempGraph.SetData(tempData?.Where(sd => Service.IsExtLocation(sd.LocationId)).ToList());
        _humiGraph.SetData(data.Data.GetValueOrDefault("humi"));
        _presGraph.SetData(data.Data.GetValueOrDefault("pres"));
        _co2Graph.SetData(data.Data.GetValueOrDefault("co2 "));
        _luxGraph.SetData(data.Data.GetValueOrDefault("lux "));
    }
}

internal sealed class WatGraphsView : GraphsView
{
    private readonly Graph _pressureGraph;
    private readonly Graph _currentGraph;

    internal WatGraphsView(ISmartHomeService service) : base(service, ["Pressure", "Current"])
    {
        _pressureGraph = Graphs[0];
        _currentGraph = Graphs[1];
    }

    internal override void Refresh(SensorDataResult data)
    {
        _pressureGraph.SetData(data.Data.GetValueOrDefault("pres"));
        _currentGraph.SetData(data.Data.GetValueOrDefault("icc "));
    }
}

internal sealed class EleGraphsView : GraphsView
{
    private readonly Graph _powerGraph;
    private readonly Graph _voltageGraph;

    internal EleGraphsView(ISmartHomeService service) : base(service, ["Power", "Voltage"])
    {
        _powerGraph = Graphs[0];
        _voltageGraph = Graphs[1];
    }

    internal override void Refresh(SensorDataResult data)
    {
        _powerGraph.SetData(data.Data.GetValueOrDefault("pwr "));
        _voltageGraph.SetData(data.Data.GetValueOrDefault("vcc "));
    }
}